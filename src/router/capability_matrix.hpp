#pragma once

#include "core/types.hpp"

#include <algorithm>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace evoclaw::router {

struct CapabilityCell {
    AgentId agent_id;
    std::string intent_tag;
    double score = 0.0;
    int usage_count = 0;
    Timestamp last_used = now();
};

class CapabilityMatrix {
public:
    void update(const AgentId& agent, const std::string& intent, double score) {
        std::unique_lock lock(mutex_);
        auto& cells = matrix_[intent];

        auto it = std::find_if(cells.begin(), cells.end(), [&](const CapabilityCell& cell) {
            return cell.agent_id == agent;
        });

        if (it == cells.end()) {
            cells.push_back(CapabilityCell{
                .agent_id = agent,
                .intent_tag = intent,
                .score = score,
                .usage_count = 1,
                .last_used = now(),
            });
            return;
        }

        const double weighted_total = (it->score * static_cast<double>(it->usage_count)) + score;
        ++it->usage_count;
        it->score = weighted_total / static_cast<double>(it->usage_count);
        it->last_used = now();
    }

    [[nodiscard]] std::optional<AgentId> best_for(const std::string& intent) const {
        std::shared_lock lock(mutex_);
        const auto it = matrix_.find(intent);
        if (it == matrix_.end() || it->second.empty()) {
            return std::nullopt;
        }

        const auto best_it = std::max_element(it->second.begin(), it->second.end(),
                                              [](const CapabilityCell& lhs, const CapabilityCell& rhs) {
                                                  if (lhs.score == rhs.score) {
                                                      return lhs.usage_count < rhs.usage_count;
                                                  }
                                                  return lhs.score < rhs.score;
                                              });
        return best_it->agent_id;
    }

    [[nodiscard]] std::vector<AgentId> candidates_for(const std::string& intent) const {
        std::shared_lock lock(mutex_);
        const auto it = matrix_.find(intent);
        if (it == matrix_.end() || it->second.empty()) {
            return {};
        }

        std::vector<CapabilityCell> sorted = it->second;
        std::sort(sorted.begin(), sorted.end(), [](const CapabilityCell& lhs, const CapabilityCell& rhs) {
            if (lhs.score == rhs.score) {
                return lhs.usage_count > rhs.usage_count;
            }
            return lhs.score > rhs.score;
        });

        std::vector<AgentId> candidates;
        candidates.reserve(sorted.size());
        for (const auto& cell : sorted) {
            candidates.push_back(cell.agent_id);
        }
        return candidates;
    }

private:
    std::unordered_map<std::string, std::vector<CapabilityCell>> matrix_;
    mutable std::shared_mutex mutex_;
};

} // namespace evoclaw::router
