#include "router/router.hpp"

#include "zone/zone_manager.hpp"

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

std::vector<std::string> read_string_list(const nlohmann::json& value) {
    std::vector<std::string> values;
    if (value.is_string()) {
        const auto parsed = value.get<std::string>();
        if (!parsed.empty()) {
            values.push_back(parsed);
        }
        return values;
    }

    if (!value.is_array()) {
        return values;
    }

    for (const auto& item : value) {
        if (item.is_string()) {
            const auto parsed = item.get<std::string>();
            if (!parsed.empty()) {
                values.push_back(parsed);
            }
        }
    }
    return values;
}

void append_if_present(std::vector<std::string>* out, const nlohmann::json& context, const std::string& key) {
    if (!context.contains(key)) {
        return;
    }
    auto parsed = read_string_list(context[key]);
    out->insert(out->end(), parsed.begin(), parsed.end());
}

bool is_subset_of(const std::vector<std::string>& subset, const std::vector<std::string>& superset) {
    for (const auto& item : subset) {
        if (std::find(superset.begin(), superset.end(), item) == superset.end()) {
            return false;
        }
    }
    return true;
}

bool passes_airbag_runtime_check(const agent::Agent& agent, const agent::Task& task) {
    const auto& airbag = agent.contract().airbag;

    std::vector<std::string> requested_permissions;
    append_if_present(&requested_permissions, task.context, "permissions");
    append_if_present(&requested_permissions, task.context, "required_permissions");
    append_if_present(&requested_permissions, task.context, "requested_permissions");

    std::vector<std::string> requested_scopes;
    append_if_present(&requested_scopes, task.context, "scope");
    append_if_present(&requested_scopes, task.context, "requested_scope");
    append_if_present(&requested_scopes, task.context, "blast_radius_scope");

    if (airbag.level == umi::AirbagLevel::MAXIMUM) {
        if (!requested_permissions.empty()) {
            return false;
        }

        if (!requested_scopes.empty()) {
            if (airbag.blast_radius_scope.empty()) {
                return false;
            }
            return is_subset_of(requested_scopes, airbag.blast_radius_scope);
        }
        return true;
    }

    if (!airbag.permission_whitelist.empty() &&
        !is_subset_of(requested_permissions, airbag.permission_whitelist)) {
        return false;
    }

    if (!airbag.blast_radius_scope.empty() &&
        !is_subset_of(requested_scopes, airbag.blast_radius_scope)) {
        return false;
    }

    if (task.context.contains("requires_reversible") &&
        task.context["requires_reversible"].is_boolean() &&
        task.context["requires_reversible"].get<bool>() &&
        !airbag.reversible) {
        return false;
    }

    return true;
}

} // namespace

Router::Router(RoutingConfig config)
    : config_(std::move(config)),
      rng_(std::random_device{}()) {}

void Router::set_zone_manager(const zone::ZoneManager* zone_manager) {
    zone_manager_ = zone_manager;
}

void Router::configure_cold_start_perturbation(std::vector<AgentId> agent_ids, const double traffic_ratio) {
    perturbation_targets_.clear();
    for (auto& agent_id : agent_ids) {
        if (!agent_id.empty()) {
            perturbation_targets_.insert(std::move(agent_id));
        }
    }
    perturbation_ratio_ = std::clamp(traffic_ratio, 0.0, 1.0);
}

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
    route_count_by_agent_.try_emplace(agent_id, 0U);

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

    std::vector<AgentId> allowed_candidates;
    allowed_candidates.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        const auto agent_it = agents_.find(candidate);
        if (agent_it == agents_.end() || !agent_it->second) {
            continue;
        }
        if (passes_airbag_runtime_check(*agent_it->second, task) &&
            can_receive_task(candidate, task)) {
            allowed_candidates.push_back(candidate);
        }
    }

    if (allowed_candidates.empty()) {
        return std::nullopt;
    }

    AgentId chosen = pick_candidate(intent, std::move(allowed_candidates));
    if (chosen.empty()) {
        return std::nullopt;
    }

    double rounds = 1.0;
    if (task.context.contains("task_rounds") && task.context["task_rounds"].is_number()) {
        rounds = std::max(1.0, task.context["task_rounds"].get<double>());
    } else if (task.context.contains("rounds") && task.context["rounds"].is_number()) {
        rounds = std::max(1.0, task.context["rounds"].get<double>());
    }
    record_routing_observation(chosen, rounds);

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
    const AgentId selected = pick_candidate(intent, std::move(candidates));
    if (!selected.empty()) {
        record_routing_observation(selected, 1.0);
    }
    return selected;
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

    if (!perturbation_targets_.empty() && perturbation_ratio_ > 0.0) {
        std::vector<AgentId> perturbation_candidates;
        perturbation_candidates.reserve(candidates.size());
        for (const auto& candidate : candidates) {
            if (perturbation_targets_.contains(candidate)) {
                perturbation_candidates.push_back(candidate);
            }
        }

        if (!perturbation_candidates.empty()) {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            if (dist(rng_) < perturbation_ratio_) {
                const AgentId selected = choose_random(perturbation_candidates);
                ++routed_count_;
                if (is_cold_start(selected)) {
                    ++cold_start_routed_count_;
                }
                return selected;
            }
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

bool Router::can_receive_task(const AgentId& agent_id, const agent::Task& task) const {
    if (!zone_manager_) {
        return true;
    }

    const auto zone = zone_manager_->get_zone(agent_id);
    if (zone != zone::Zone::EXPLORATION) {
        return true;
    }

    if (task.context.contains("allow_exploration_zone") &&
        task.context["allow_exploration_zone"].is_boolean() &&
        task.context["allow_exploration_zone"].get<bool>()) {
        return true;
    }

    if (task.context.contains("caller_zone") && task.context["caller_zone"].is_string()) {
        const auto caller_zone = task.context["caller_zone"].get<std::string>();
        if (caller_zone == "stable") {
            return true;
        }
    }

    if (task.context.contains("internal_call") &&
        task.context["internal_call"].is_boolean() &&
        task.context["internal_call"].get<bool>()) {
        return true;
    }

    return false;
}

void Router::record_routing_observation(const AgentId& agent_id, const double task_rounds) {
    if (agent_id.empty()) {
        return;
    }

    route_count_by_agent_[agent_id] += 1U;
    last_routed_by_agent_[agent_id] = now();
    total_task_rounds_ += std::max(task_rounds, 1.0);
    ++observed_task_count_;
}

std::vector<AgentId> Router::registered_agents() const {
    std::vector<AgentId> ids;
    ids.reserve(agents_.size());
    for (const auto& [agent_id, _] : agents_) {
        ids.push_back(agent_id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::unordered_map<AgentId, std::size_t> Router::routing_counts() const {
    return route_count_by_agent_;
}

std::unordered_map<AgentId, Timestamp> Router::last_routed_at() const {
    return last_routed_by_agent_;
}

std::size_t Router::routed_count() const {
    return routed_count_;
}

std::size_t Router::cold_start_routed_count() const {
    return cold_start_routed_count_;
}

double Router::average_task_rounds() const {
    if (observed_task_count_ == 0U) {
        return 0.0;
    }
    return total_task_rounds_ / static_cast<double>(observed_task_count_);
}

nlohmann::json Router::get_capability_matrix() const {
    // 简化返回，实际应该序列化 matrix_ 的内容
    nlohmann::json j = nlohmann::json::object();
    for (const auto& [agent_id, agent_ptr] : agents_) {
        j[agent_id] = {
            {"contract", agent_ptr->contract().module_id},
            {"role", agent_ptr->role()}
        };
    }
    return j;
}

} // namespace evoclaw::router
