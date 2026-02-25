#pragma once

#include "core/types.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace evoclaw::evolution {

enum class TensionType {
    KPI_DECLINE,
    REPEATED_FAILURE,
    REFLECTION_SUGGESTION,
    MANUAL
};

struct Tension {
    std::string id;
    TensionType type = TensionType::MANUAL;
    AgentId source_agent;
    std::string description;
    std::string severity;
    nlohmann::json metadata = nlohmann::json::object();
};

} // namespace evoclaw::evolution
