#pragma once

#include "core/types.hpp"

#include <string>

namespace evoclaw::zone {

enum class Zone {
    STABLE,
    EXPLORATION
};

struct ZonePolicy {
    Zone zone = Zone::STABLE;
    double token_budget_ratio = 1.0;
    int min_success_count = 20;
    double min_success_rate = 0.75;
    int retirement_days = 7;
    bool can_face_user = true;
};

[[nodiscard]] inline std::string zone_to_string(const Zone zone) {
    switch (zone) {
        case Zone::STABLE:
            return "stable";
        case Zone::EXPLORATION:
            return "exploration";
    }
    return "stable";
}

} // namespace evoclaw::zone
