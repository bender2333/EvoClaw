#include "entropy/entropy_monitor.hpp"

#include "core/types.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <utility>

namespace evoclaw::entropy {
namespace {

double clamp01(const double value) {
    return std::clamp(value, 0.0, 1.0);
}

nlohmann::json action_to_json(const AntiEntropyAction& action) {
    return {
        {"type", action.type},
        {"description", action.description},
        {"details", action.details}
    };
}

nlohmann::json metrics_to_json(const EntropyMetrics& metrics) {
    return {
        {"active_agent_count", metrics.active_agent_count},
        {"active_primitive_count", metrics.active_primitive_count},
        {"routing_concentration", metrics.routing_concentration},
        {"avg_task_rounds", metrics.avg_task_rounds},
        {"entropy_score", metrics.entropy_score}
    };
}

} // namespace

EntropyMonitor::EntropyMonitor(const router::Router& router)
    : router_(router) {}

EntropyMetrics EntropyMonitor::measure() const {
    EntropyMetrics metrics;

    const std::vector<AgentId> registered_agents = router_.registered_agents();
    const auto routing_counts = router_.routing_counts();
    const std::size_t total_routed = router_.routed_count();

    metrics.active_agent_count = static_cast<int>(registered_agents.size());

    int active_primitives = 0;
    std::size_t max_routes = 0;
    for (const auto& [agent_id, count] : routing_counts) {
        (void)agent_id;
        if (count > 0U) {
            ++active_primitives;
        }
        if (count > max_routes) {
            max_routes = count;
        }
    }
    metrics.active_primitive_count = active_primitives;

    if (total_routed > 0U) {
        metrics.routing_concentration = static_cast<double>(max_routes) / static_cast<double>(total_routed);
    } else {
        metrics.routing_concentration = 0.0;
    }

    metrics.avg_task_rounds = router_.average_task_rounds();

    const double idle_ratio = (metrics.active_agent_count <= 0)
                                  ? 0.0
                                  : 1.0 - static_cast<double>(metrics.active_primitive_count) /
                                              static_cast<double>(metrics.active_agent_count);
    const double rounds_pressure = clamp01((metrics.avg_task_rounds - 1.0) / 4.0);

    metrics.entropy_score = clamp01(0.55 * metrics.routing_concentration +
                                    0.30 * idle_ratio +
                                    0.15 * rounds_pressure);
    return metrics;
}

std::vector<AntiEntropyAction> EntropyMonitor::suggest_actions(const double threshold) const {
    const EntropyMetrics metrics = measure();
    if (metrics.entropy_score + 1e-9 < threshold) {
        return {};
    }

    std::vector<AntiEntropyAction> actions;

    if (metrics.routing_concentration > 0.60) {
        actions.push_back({
            "module_consolidation",
            "Routing is concentrated on too few agents; consolidate overlapping modules.",
            {
                {"routing_concentration", metrics.routing_concentration},
                {"suggested_target_count", std::max(1, metrics.active_agent_count / 2)}
            }
        });
    }

    if (metrics.active_agent_count > 0 &&
        metrics.active_primitive_count < metrics.active_agent_count) {
        actions.push_back({
            "cold_start_perturbation",
            "Route a small slice of traffic to long-idle agents for recalibration.",
            {
                {"active_agent_count", metrics.active_agent_count},
                {"active_primitive_count", metrics.active_primitive_count},
                {"traffic_ratio_hint", 0.05}
            }
        });
    }

    if (metrics.avg_task_rounds > 2.5) {
        actions.push_back({
            "primitive_merge",
            "Task round-trips are high; merge frequently chained primitives.",
            {
                {"avg_task_rounds", metrics.avg_task_rounds},
                {"merge_priority", "high"}
            }
        });
    }

    if (actions.empty()) {
        actions.push_back({
            "module_consolidation",
            "Entropy is high; perform a lightweight module consolidation pass.",
            {{"entropy_score", metrics.entropy_score}}
        });
    }

    return actions;
}

void EntropyMonitor::apply_cold_start_perturbation(router::Router& router, const double traffic_ratio) {
    const auto agents = router.registered_agents();
    const auto counts = router.routing_counts();
    const auto last_routed = router.last_routed_at();

    std::vector<AgentId> targets;
    const auto current_time = now();
    const auto stale_window = std::chrono::hours(24LL * 7);

    for (const auto& agent_id : agents) {
        const auto count_it = counts.find(agent_id);
        const std::size_t routed_count = (count_it == counts.end()) ? 0U : count_it->second;
        if (routed_count == 0U) {
            targets.push_back(agent_id);
            continue;
        }

        const auto last_it = last_routed.find(agent_id);
        if (last_it == last_routed.end()) {
            targets.push_back(agent_id);
            continue;
        }
        if (current_time - last_it->second >= stale_window) {
            targets.push_back(agent_id);
        }
    }

    router.configure_cold_start_perturbation(targets, std::clamp(traffic_ratio, 0.0, 1.0));
}

nlohmann::json EntropyMonitor::status() const {
    const EntropyMetrics metrics = measure();
    const auto actions = suggest_actions(warning_threshold_);

    nlohmann::json payload;
    payload["metrics"] = metrics_to_json(metrics);
    payload["warning_threshold"] = warning_threshold_;
    payload["actions"] = nlohmann::json::array();
    for (const auto& action : actions) {
        payload["actions"].push_back(action_to_json(action));
    }

    return payload;
}

} // namespace evoclaw::entropy
