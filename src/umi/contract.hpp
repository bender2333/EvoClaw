#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace evoclaw::umi {

// 能力声明
struct CapabilityProfile {
    std::vector<std::string> intent_tags;
    std::vector<std::string> required_tools;
    double estimated_cost_token = 0.0;
    double success_rate_threshold = 0.0;
};

// 安全声明
enum class AirbagLevel {
    NONE,
    BASIC,
    ENHANCED,
    MAXIMUM
};

NLOHMANN_JSON_SERIALIZE_ENUM(AirbagLevel, {
    {AirbagLevel::NONE, "none"},
    {AirbagLevel::BASIC, "basic"},
    {AirbagLevel::ENHANCED, "enhanced"},
    {AirbagLevel::MAXIMUM, "maximum"}
})

struct AirbagSpec {
    AirbagLevel level = AirbagLevel::NONE;
    std::vector<std::string> permission_whitelist;
    std::vector<std::string> blast_radius_scope;
    bool reversible = true;
};

// UMI 契约（所有模块必须实现）
struct ModuleContract {
    std::string module_id;
    std::string version;
    CapabilityProfile capability;
    AirbagSpec airbag;
    nlohmann::json cost_model = nlohmann::json::object();

    // 验证契约完整性
    [[nodiscard]] bool validate() const;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CapabilityProfile,
                                   intent_tags,
                                   required_tools,
                                   estimated_cost_token,
                                   success_rate_threshold)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AirbagSpec,
                                   level,
                                   permission_whitelist,
                                   blast_radius_scope,
                                   reversible)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModuleContract,
                                   module_id,
                                   version,
                                   capability,
                                   airbag,
                                   cost_model)

inline bool ModuleContract::validate() const {
    if (module_id.empty() || version.empty()) {
        return false;
    }
    if (capability.intent_tags.empty()) {
        return false;
    }
    if (capability.estimated_cost_token < 0.0) {
        return false;
    }
    if (capability.success_rate_threshold < 0.0 || capability.success_rate_threshold > 1.0) {
        return false;
    }
    if (airbag.level == AirbagLevel::MAXIMUM && !airbag.permission_whitelist.empty()) {
        return false;
    }
    if (!(cost_model.is_null() || cost_model.is_object())) {
        return false;
    }

    return true;
}

} // namespace evoclaw::umi
