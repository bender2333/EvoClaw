#pragma once

#include "core/types.hpp"

#include <nlohmann/json.hpp>

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

    [[nodiscard]] nlohmann::json snapshot() const {
        std::shared_lock lock(mutex_);

        nlohmann::json matrix = nlohmann::json::object();
        for (const auto& [intent, cells] : matrix_) {
            for (const auto& cell : cells) {
                if (cell.agent_id.empty() || intent.empty()) {
                    continue;
                }
                matrix[cell.agent_id][intent] = {
                    {"score", cell.score},
                    {"observations", cell.usage_count}
                };
            }
        }
        return matrix;
    }

    void load_snapshot(const nlohmann::json& matrix) {
        if (!matrix.is_object()) {
            return;
        }

        std::unique_lock lock(mutex_);
        matrix_.clear();

        for (auto agent_it = matrix.begin(); agent_it != matrix.end(); ++agent_it) {
            const auto& agent_id = agent_it.key();
            const auto& intents_obj = agent_it.value();
            if (agent_id.empty() || !intents_obj.is_object()) {
                continue;
            }

            for (auto intent_it = intents_obj.begin(); intent_it != intents_obj.end(); ++intent_it) {
                const auto& intent = intent_it.key();
                const auto& cell_obj = intent_it.value();
                if (intent.empty() || !cell_obj.is_object()) {
                    continue;
                }

                CapabilityCell cell;
                cell.agent_id = agent_id;
                cell.intent_tag = intent;
                cell.score = cell_obj.value("score", 0.0);
                cell.usage_count = std::max(1, cell_obj.value("observations", 1));
                cell.last_used = now();
                matrix_[intent].push_back(std::move(cell));
            }
        }
    }

private:
    std::unordered_map<std::string, std::vector<CapabilityCell>> matrix_;
    mutable std::shared_mutex mutex_;
};

} // namespace evoclaw::router
