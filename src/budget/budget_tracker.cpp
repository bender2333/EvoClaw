#include "budget/budget_tracker.hpp"

#include <algorithm>

namespace evoclaw::budget {

void BudgetTracker::record(const AgentId& agent, int prompt_tokens, int completion_tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    AgentUsage& usage = usage_[agent];
    usage.prompt_tokens += std::max(prompt_tokens, 0);
    usage.completion_tokens += std::max(completion_tokens, 0);
    usage.calls += 1;
}

int BudgetTracker::total_tokens() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int total = 0;
    for (const auto& [_, usage] : usage_) {
        (void)_;
        total += std::max(usage.prompt_tokens, 0) + std::max(usage.completion_tokens, 0);
    }
    return total;
}

int BudgetTracker::agent_tokens(const AgentId& agent) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = usage_.find(agent);
    if (it == usage_.end()) {
        return 0;
    }
    return std::max(it->second.prompt_tokens, 0) + std::max(it->second.completion_tokens, 0);
}

nlohmann::json BudgetTracker::report() const {
    std::lock_guard<std::mutex> lock(mutex_);

    nlohmann::json agents = nlohmann::json::object();
    int total = 0;

    for (const auto& [agent_id, usage] : usage_) {
        const int agent_total = std::max(usage.prompt_tokens, 0) + std::max(usage.completion_tokens, 0);
        total += agent_total;
        agents[agent_id] = {
            {"prompt_tokens", usage.prompt_tokens},
            {"completion_tokens", usage.completion_tokens},
            {"total_tokens", agent_total},
            {"calls", usage.calls}
        };
    }

    return {
        {"global_limit", global_limit_},
        {"total_tokens", total},
        {"global_budget_exceeded", global_limit_ > 0 && total > global_limit_},
        {"agents", std::move(agents)}
    };
}

void BudgetTracker::set_global_limit(const int max_tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    global_limit_ = std::max(max_tokens, 0);
}

bool BudgetTracker::is_global_budget_exceeded() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (global_limit_ <= 0) {
        return false;
    }

    int total = 0;
    for (const auto& [_, usage] : usage_) {
        (void)_;
        total += std::max(usage.prompt_tokens, 0) + std::max(usage.completion_tokens, 0);
    }
    return total > global_limit_;
}

} // namespace evoclaw::budget
