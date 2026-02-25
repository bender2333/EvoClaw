#pragma once

#include "core/types.hpp"

#include <nlohmann/json.hpp>

#include <mutex>
#include <vector>

namespace evoclaw::evolution {

class EvolutionBudget {
public:
    explicit EvolutionBudget(int max_cycles_per_hour, int token_budget);

    [[nodiscard]] bool can_evolve() const;
    void record_cycle();
    void record_tokens(int tokens);
    [[nodiscard]] nlohmann::json status() const;

private:
    int max_cycles_per_hour_;
    int token_budget_;
    int tokens_used_ = 0;
    std::vector<Timestamp> cycle_history_;
    mutable std::mutex mutex_;
};

} // namespace evoclaw::evolution
