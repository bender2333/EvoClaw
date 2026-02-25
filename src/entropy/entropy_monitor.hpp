#pragma once

#include "router/router.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace evoclaw::entropy {

struct EntropyMetrics {
    int active_agent_count = 0;
    int active_primitive_count = 0;
    double routing_concentration = 0.0;
    double avg_task_rounds = 0.0;
    double entropy_score = 0.0;
};

struct AntiEntropyAction {
    std::string type;
    std::string description;
    nlohmann::json details = nlohmann::json::object();
};

class EntropyMonitor {
public:
    explicit EntropyMonitor(const router::Router& router);

    [[nodiscard]] EntropyMetrics measure() const;
    [[nodiscard]] std::vector<AntiEntropyAction> suggest_actions(double threshold = 0.7) const;
    void apply_cold_start_perturbation(router::Router& router, double traffic_ratio = 0.05);
    [[nodiscard]] nlohmann::json status() const;

private:
    const router::Router& router_;
    double warning_threshold_ = 0.7;
};

} // namespace evoclaw::entropy
