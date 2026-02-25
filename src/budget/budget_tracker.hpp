#pragma once

#include "core/types.hpp"

#include <nlohmann/json.hpp>

#include <mutex>
#include <unordered_map>

namespace evoclaw::budget {

class BudgetTracker {
public:
    void record(const AgentId& agent, int prompt_tokens, int completion_tokens);

    [[nodiscard]] int total_tokens() const;
    [[nodiscard]] int agent_tokens(const AgentId& agent) const;
    [[nodiscard]] nlohmann::json report() const;

    void set_global_limit(int max_tokens);
    [[nodiscard]] bool is_global_budget_exceeded() const;

private:
    struct AgentUsage {
        int prompt_tokens = 0;
        int completion_tokens = 0;
        int calls = 0;
    };

    std::unordered_map<AgentId, AgentUsage> usage_;
    int global_limit_ = 0; // 0 = unlimited
    mutable std::mutex mutex_;
};

} // namespace evoclaw::budget
