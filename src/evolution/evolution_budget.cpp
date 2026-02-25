#include "evolution/evolution_budget.hpp"

#include <algorithm>
#include <chrono>

namespace evoclaw::evolution {
namespace {

using Clock = std::chrono::system_clock;

Timestamp one_hour_ago(const Timestamp reference) {
    return reference - std::chrono::hours(1);
}

std::size_t cycles_within_last_hour(const std::vector<Timestamp>& history, const Timestamp reference) {
    const Timestamp cutoff = one_hour_ago(reference);
    std::size_t count = 0;
    for (const auto& ts : history) {
        if (ts >= cutoff) {
            ++count;
        }
    }
    return count;
}

void prune_old_cycles(std::vector<Timestamp>& history, const Timestamp reference) {
    const Timestamp cutoff = one_hour_ago(reference);
    history.erase(
        std::remove_if(history.begin(), history.end(), [cutoff](const Timestamp ts) {
            return ts < cutoff;
        }),
        history.end());
}

} // namespace

EvolutionBudget::EvolutionBudget(const int max_cycles_per_hour, const int token_budget)
    : max_cycles_per_hour_(std::max(max_cycles_per_hour, 0)),
      token_budget_(std::max(token_budget, 0)) {}

bool EvolutionBudget::can_evolve() const {
    std::lock_guard<std::mutex> lock(mutex_);

    const Timestamp reference = Clock::now();
    const std::size_t recent_cycles = cycles_within_last_hour(cycle_history_, reference);

    const bool cycles_ok =
        max_cycles_per_hour_ <= 0 || recent_cycles < static_cast<std::size_t>(max_cycles_per_hour_);
    const bool tokens_ok = token_budget_ <= 0 || tokens_used_ < token_budget_;
    return cycles_ok && tokens_ok;
}

void EvolutionBudget::record_cycle() {
    std::lock_guard<std::mutex> lock(mutex_);

    const Timestamp reference = Clock::now();
    prune_old_cycles(cycle_history_, reference);
    cycle_history_.push_back(reference);
    tokens_used_ = 0;
}

void EvolutionBudget::record_tokens(const int tokens) {
    if (tokens <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    tokens_used_ += tokens;
}

nlohmann::json EvolutionBudget::status() const {
    std::lock_guard<std::mutex> lock(mutex_);

    const Timestamp reference = Clock::now();
    const std::size_t recent_cycles = cycles_within_last_hour(cycle_history_, reference);
    const bool cycles_ok =
        max_cycles_per_hour_ <= 0 || recent_cycles < static_cast<std::size_t>(max_cycles_per_hour_);
    const bool tokens_ok = token_budget_ <= 0 || tokens_used_ < token_budget_;
    const int tokens_remaining = token_budget_ > 0 ? std::max(token_budget_ - tokens_used_, 0) : -1;

    return {
        {"max_cycles_per_hour", max_cycles_per_hour_},
        {"cycles_last_hour", recent_cycles},
        {"token_budget", token_budget_},
        {"tokens_used", tokens_used_},
        {"tokens_remaining", tokens_remaining},
        {"can_evolve", cycles_ok && tokens_ok}
    };
}

} // namespace evoclaw::evolution
