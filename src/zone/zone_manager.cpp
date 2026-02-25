#include "zone/zone_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace evoclaw::zone {
namespace {

std::int64_t to_epoch_seconds(const Timestamp ts) {
    return std::chrono::duration_cast<std::chrono::seconds>(ts.time_since_epoch()).count();
}

Timestamp add_days(const Timestamp ts, const int days) {
    if (days <= 0) {
        return ts;
    }
    return ts + std::chrono::hours(24LL * static_cast<long long>(days));
}

nlohmann::json policy_to_json(const ZonePolicy& policy) {
    return {
        {"zone", zone_to_string(policy.zone)},
        {"token_budget_ratio", policy.token_budget_ratio},
        {"min_success_count", policy.min_success_count},
        {"min_success_rate", policy.min_success_rate},
        {"retirement_days", policy.retirement_days},
        {"can_face_user", policy.can_face_user}
    };
}

} // namespace

ZonePolicy ZoneManager::policy_for_zone(const Zone zone) {
    if (zone == Zone::EXPLORATION) {
        ZonePolicy policy;
        policy.zone = Zone::EXPLORATION;
        policy.token_budget_ratio = 0.7;
        policy.min_success_count = 20;
        policy.min_success_rate = 0.75;
        policy.retirement_days = 7;
        policy.can_face_user = false;
        return policy;
    }

    ZonePolicy policy;
    policy.zone = Zone::STABLE;
    policy.token_budget_ratio = 1.0;
    policy.min_success_count = 20;
    policy.min_success_rate = 0.75;
    policy.retirement_days = 7;
    policy.can_face_user = true;
    return policy;
}

void ZoneManager::assign_zone(const AgentId& agent, const Zone zone) {
    if (agent.empty()) {
        return;
    }

    std::scoped_lock lock(mutex_);
    zones_[agent] = zone;
    stats_.try_emplace(agent, AgentStats{});
}

Zone ZoneManager::get_zone(const AgentId& agent) const {
    std::scoped_lock lock(mutex_);
    const auto it = zones_.find(agent);
    if (it == zones_.end()) {
        return Zone::STABLE;
    }
    return it->second;
}

ZonePolicy ZoneManager::get_policy(const AgentId& agent) const {
    return policy_for_zone(get_zone(agent));
}

bool ZoneManager::is_promotion_eligible(const AgentId& agent) const {
    if (agent.empty()) {
        return false;
    }

    std::scoped_lock lock(mutex_);
    const auto zone_it = zones_.find(agent);
    if (zone_it == zones_.end() || zone_it->second != Zone::EXPLORATION) {
        return false;
    }

    const auto stats_it = stats_.find(agent);
    if (stats_it == stats_.end()) {
        return false;
    }

    const ZonePolicy policy = policy_for_zone(zone_it->second);
    const AgentStats& stats = stats_it->second;
    if (stats.success_count < policy.min_success_count || stats.total_count <= 0) {
        return false;
    }

    const double success_rate = static_cast<double>(stats.success_count) /
                                static_cast<double>(stats.total_count);
    return success_rate + 1e-9 >= policy.min_success_rate;
}

void ZoneManager::promote_to_stable(const AgentId& agent) {
    if (agent.empty()) {
        return;
    }

    std::scoped_lock lock(mutex_);
    zones_[agent] = Zone::STABLE;
    stats_.try_emplace(agent, AgentStats{});
    promoted_at_[agent] = now();
}

void ZoneManager::record_result(const AgentId& agent, const bool success) {
    if (agent.empty()) {
        return;
    }

    std::scoped_lock lock(mutex_);
    AgentStats& stats = stats_[agent];
    ++stats.total_count;
    if (success) {
        ++stats.success_count;
    }
}

nlohmann::json ZoneManager::status() const {
    std::scoped_lock lock(mutex_);

    nlohmann::json payload;
    payload["agents"] = nlohmann::json::array();
    payload["summary"] = {
        {"stable", 0},
        {"exploration", 0}
    };

    for (const auto& [agent_id, zone] : zones_) {
        const ZonePolicy policy = policy_for_zone(zone);
        const auto stats_it = stats_.find(agent_id);
        const AgentStats stats = (stats_it == stats_.end()) ? AgentStats{} : stats_it->second;

        double success_rate = 0.0;
        if (stats.total_count > 0) {
            success_rate = static_cast<double>(stats.success_count) / static_cast<double>(stats.total_count);
        }
        const bool promotion_eligible =
            zone == Zone::EXPLORATION &&
            stats.success_count >= policy.min_success_count &&
            stats.total_count > 0 &&
            success_rate + 1e-9 >= policy.min_success_rate;

        nlohmann::json promoted = nlohmann::json::object();
        const auto promoted_it = promoted_at_.find(agent_id);
        if (promoted_it != promoted_at_.end()) {
            promoted = {
                {"promoted_at_s", to_epoch_seconds(promoted_it->second)},
                {"retirement_until_s", to_epoch_seconds(add_days(promoted_it->second, policy.retirement_days))}
            };
        }

        payload["agents"].push_back({
            {"agent_id", agent_id},
            {"zone", zone_to_string(zone)},
            {"policy", policy_to_json(policy)},
            {"metrics", {
                {"success_count", stats.success_count},
                {"total_count", stats.total_count},
                {"success_rate", success_rate},
                {"promotion_eligible", promotion_eligible}
            }},
            {"promotion", promoted}
        });

        if (zone == Zone::STABLE) {
            payload["summary"]["stable"] = payload["summary"]["stable"].get<int>() + 1;
        } else {
            payload["summary"]["exploration"] = payload["summary"]["exploration"].get<int>() + 1;
        }
    }

    std::sort(payload["agents"].begin(), payload["agents"].end(), [](const nlohmann::json& lhs,
                                                                     const nlohmann::json& rhs) {
        return lhs.value("agent_id", "") < rhs.value("agent_id", "");
    });

    return payload;
}

} // namespace evoclaw::zone
