#pragma once

#include "core/types.hpp"

#include <string>
#include <vector>

namespace evoclaw::compiler {

struct Pattern {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> step_sequence;
    int observation_count = 0;
    double avg_success_rate = 0.0;
    Timestamp created_at{};
    Timestamp last_seen{};
};

} // namespace evoclaw::compiler
