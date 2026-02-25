#pragma once

#include "core/types.hpp"
#include "zone/zone.hpp"

#include <nlohmann/json.hpp>

#include <mutex>
#include <unordered_map>

namespace evoclaw::zone {

class ZoneManager {
public:
    void assign_zone(const AgentId& agent, Zone zone);
    [[nodiscard]] Zone get_zone(const AgentId& agent) const;
    [[nodiscard]] ZonePolicy get_policy(const AgentId& agent) const;

    [[nodiscard]] bool is_promotion_eligible(const AgentId& agent) const;
    void promote_to_stable(const AgentId& agent);

    void record_result(const AgentId& agent, bool success);

    [[nodiscard]] nlohmann::json status() const;

private:
    struct AgentStats {
        int success_count = 0;
        int total_count = 0;
    };

    [[nodiscard]] static ZonePolicy policy_for_zone(Zone zone);

    mutable std::mutex mutex_;
    std::unordered_map<AgentId, Zone> zones_;
    std::unordered_map<AgentId, AgentStats> stats_;
    std::unordered_map<AgentId, Timestamp> promoted_at_;
};

} // namespace evoclaw::zone
