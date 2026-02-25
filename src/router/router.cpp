#include "router/router.hpp"

#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <utility>

namespace evoclaw::router {
namespace {

std::string derive_intent(const agent::Task& task) {
    if (task.context.contains("intent") && task.context["intent"].is_string()) {
        const auto intent = task.context["intent"].get<std::string>();
        if (!intent.empty()) {
            return intent;
        }
    }
    if (task.context.contains("intent_tag") && task.context["intent_tag"].is_string()) {
        const auto intent = task.context["intent_tag"].get<std::string>();
        if (!intent.empty()) {
            return intent;
        }
    }
    if (task.context.contains("intent_tags") && task.context["intent_tags"].is_array()) {
        for (const auto& tag : task.context["intent_tags"]) {
            if (tag.is_string()) {
                const auto intent = tag.get<std::string>();
                if (!intent.empty()) {
                    return intent;
                }
            }
        }
    }
    return task.description;
}

bool has_any_tag(const std::vector<std::string>& haystack, const std::vector<std::string>& needles) {
    for (const auto& needle : needles) {
        if (std::find(haystack.begin(), haystack.end(), needle) != haystack.end()) {
            return true;
        }
    }
    return false;
}

bool has_all_tools(const std::vector<std::string>& declared, const std::vector<std::string>& required) {
    for (const auto& tool : required) {
        if (std::find(declared.begin(), declared.end(), tool) == declared.end()) {
            return false;
        }
    }
    return true;
}

} // namespace

Router::Router(RoutingConfig config)
    : config_(std::move(config)),
      rng_(std::random_device{}()) {}

void Router::register_agent(const std::shared_ptr<agent::Agent>& agent) {
    register_agent(agent, now());
}

void Router::register_agent(const std::shared_ptr<agent::Agent>& agent, Timestamp registered_at) {
    if (!agent) {
        return;
    }

    const AgentId agent_id = agent->id();
    agents_[agent_id] = agent;
    registration_time_[agent_id] = registered_at;

    const auto& capability = agent->contract().capability;
    for (const auto& intent : capability.intent_tags) {
        matrix_.update(agent_id, intent, capability.success_rate_threshold);
    }
}

std::optional<AgentId> Router::route(const agent::Task& task) {
    if (agents_.empty()) {
        return std::nullopt;
    }

    umi::CapabilityProfile required;
    const std::string intent = derive_intent(task);
    if (!intent.empty()) {
        required.intent_tags.push_back(intent);
    }

    if (task.context.contains("required_tools") && task.context["required_tools"].is_array()) {
        for (const auto& tool : task.context["required_tools"]) {
            if (tool.is_string()) {
                required.required_tools.push_back(tool.get<std::string>());
            }
        }
    }
    if (task.context.contains("estimated_cost_token") && task.context["estimated_cost_token"].is_number()) {
        required.estimated_cost_token = task.context["estimated_cost_token"].get<double>();
    }
    if (task.context.contains("success_rate_threshold") && task.context["success_rate_threshold"].is_number()) {
        required.success_rate_threshold = task.context["success_rate_threshold"].get<double>();
    }

    std::vector<AgentId> candidates = match_by_contract(required);
    if (candidates.empty()) {
        candidates.reserve(agents_.size());
        for (const auto& [id, _] : agents_) {
            candidates.push_back(id);
        }
    }

    AgentId chosen = pick_candidate(intent, std::move(candidates));
    if (chosen.empty()) {
        return std::nullopt;
    }
    return chosen;
}

AgentId Router::route_with_exploration(const std::string& intent) {
    if (agents_.empty()) {
        return {};
    }

    std::vector<AgentId> candidates;
    candidates.reserve(agents_.size());
    for (const auto& [id, _] : agents_) {
        candidates.push_back(id);
    }
    return pick_candidate(intent, std::move(candidates));
}

std::vector<AgentId> Router::match_by_contract(const umi::CapabilityProfile& required) const {
    std::vector<AgentId> matches;

    for (const auto& [agent_id, agent_ptr] : agents_) {
        const auto& capability = agent_ptr->contract().capability;

        if (!required.intent_tags.empty() && !has_any_tag(capability.intent_tags, required.intent_tags)) {
            continue;
        }
        if (!required.required_tools.empty() && !has_all_tools(capability.required_tools, required.required_tools)) {
            continue;
        }
        if (required.success_rate_threshold > 0.0 &&
            capability.success_rate_threshold + 1e-9 < required.success_rate_threshold) {
            continue;
        }
        if (required.estimated_cost_token > 0.0 &&
            capability.estimated_cost_token > required.estimated_cost_token) {
            continue;
        }

        matches.push_back(agent_id);
    }

    std::sort(matches.begin(), matches.end());
    return matches;
}

bool Router::is_cold_start(const AgentId& agent) const {
    const auto it = registration_time_.find(agent);
    if (it == registration_time_.end() || config_.cold_start_days <= 0) {
        return false;
    }

    const auto cold_window = std::chrono::hours(24LL * static_cast<long long>(config_.cold_start_days));
    return (now() - it->second) <= cold_window;
}

double Router::effective_epsilon(bool has_cold_candidates) const {
    double epsilon = config_.epsilon;
    if (has_cold_candidates) {
        epsilon += config_.epsilon_boost;
    }
    epsilon = std::max(config_.epsilon_min, epsilon);
    return std::clamp(epsilon, 0.0, 1.0);
}

AgentId Router::pick_candidate(const std::string& intent, std::vector<AgentId> candidates) {
    if (candidates.empty()) {
        return {};
    }

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    const auto choose_random = [this](const std::vector<AgentId>& options) {
        std::uniform_int_distribution<std::size_t> dist(0, options.size() - 1);
        return options[dist(rng_)];
    };

    std::vector<AgentId> cold_candidates;
    for (const auto& candidate : candidates) {
        if (is_cold_start(candidate)) {
            cold_candidates.push_back(candidate);
        }
    }

    if (!cold_candidates.empty() && config_.cold_start_quota > 0.0) {
        const double cold_ratio = routed_count_ == 0
                                      ? 0.0
                                      : static_cast<double>(cold_start_routed_count_) /
                                            static_cast<double>(routed_count_);
        if (cold_ratio + 1e-9 < config_.cold_start_quota) {
            const AgentId selected = choose_random(cold_candidates);
            ++routed_count_;
            ++cold_start_routed_count_;
            return selected;
        }
    }

    AgentId greedy = candidates.front();
    if (!intent.empty()) {
        const auto ranked = matrix_.candidates_for(intent);
        std::unordered_set<AgentId> candidate_set(candidates.begin(), candidates.end());
        for (const auto& ranked_id : ranked) {
            if (candidate_set.contains(ranked_id)) {
                greedy = ranked_id;
                break;
            }
        }
    }

    const double epsilon = effective_epsilon(!cold_candidates.empty());
    const bool can_explore = candidates.size() > 1U;
    bool explore = false;
    if (can_explore && epsilon > 0.0) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        explore = dist(rng_) < epsilon;
    }

    AgentId selected = greedy;
    if (explore) {
        std::vector<AgentId> explore_pool = candidates;
        if (explore_pool.size() > 1U) {
            explore_pool.erase(std::remove(explore_pool.begin(), explore_pool.end(), greedy), explore_pool.end());
        }
        if (!explore_pool.empty()) {
            selected = choose_random(explore_pool);
        }
    }

    ++routed_count_;
    if (is_cold_start(selected)) {
        ++cold_start_routed_count_;
    }

    if (!intent.empty()) {
        const auto agent_it = agents_.find(selected);
        if (agent_it != agents_.end()) {
            matrix_.update(selected, intent, agent_it->second->contract().capability.success_rate_threshold);
        }
    }

    return selected;
}

} // namespace evoclaw::router
