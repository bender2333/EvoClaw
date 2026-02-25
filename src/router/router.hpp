#pragma once

#include "agent/agent.hpp"
#include "router/capability_matrix.hpp"
#include "umi/contract.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace evoclaw::router {

struct RoutingConfig {
    double epsilon = 0.1;
    double epsilon_boost = 0.2;
    double epsilon_min = 0.05;
    double cold_start_quota = 0.15;
    int cold_start_days = 14;
};

class Router {
public:
    explicit Router(RoutingConfig config = {});

    void register_agent(const std::shared_ptr<agent::Agent>& agent);
    void register_agent(const std::shared_ptr<agent::Agent>& agent, Timestamp registered_at);

    [[nodiscard]] std::optional<AgentId> route(const agent::Task& task);
    [[nodiscard]] AgentId route_with_exploration(const std::string& intent);
    [[nodiscard]] std::vector<AgentId> match_by_contract(const umi::CapabilityProfile& required) const;
    [[nodiscard]] nlohmann::json get_capability_matrix() const;

private:
    [[nodiscard]] bool is_cold_start(const AgentId& agent) const;
    [[nodiscard]] double effective_epsilon(bool has_cold_candidates) const;
    [[nodiscard]] AgentId pick_candidate(const std::string& intent, std::vector<AgentId> candidates);

    RoutingConfig config_;
    CapabilityMatrix matrix_;
    std::unordered_map<AgentId, std::shared_ptr<agent::Agent>> agents_;
    std::unordered_map<AgentId, Timestamp> registration_time_;

    std::mt19937 rng_;
    std::size_t routed_count_ = 0;
    std::size_t cold_start_routed_count_ = 0;
};

} // namespace evoclaw::router
