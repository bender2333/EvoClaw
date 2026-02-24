#pragma once

#include "umi/contract.hpp"

#include <regex>
#include <string>
#include <vector>

namespace evoclaw::umi {

struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
};

class ContractValidator {
public:
    [[nodiscard]] static ValidationResult validate(const ModuleContract& contract) {
        ValidationResult result;

        if (contract.module_id.empty()) {
            result.errors.emplace_back("module_id must not be empty");
        }
        if (!is_semver(contract.version)) {
            result.errors.emplace_back("version must follow semver (e.g. 1.0.0)");
        }
        if (contract.capability.intent_tags.empty()) {
            result.errors.emplace_back("capability.intent_tags must not be empty");
        }
        if (contract.capability.estimated_cost_token < 0.0) {
            result.errors.emplace_back("capability.estimated_cost_token must be >= 0");
        }
        if (contract.capability.success_rate_threshold < 0.0 ||
            contract.capability.success_rate_threshold > 1.0) {
            result.errors.emplace_back("capability.success_rate_threshold must be in [0, 1]");
        }
        if (contract.airbag.level == AirbagLevel::MAXIMUM &&
            !contract.airbag.permission_whitelist.empty()) {
            result.errors.emplace_back("airbag.permission_whitelist must be empty when level is MAXIMUM");
        }
        if (!(contract.cost_model.is_null() || contract.cost_model.is_object())) {
            result.errors.emplace_back("cost_model must be an object or null");
        }

        if (!contract.validate()) {
            result.errors.emplace_back("contract.validate() check failed");
        }

        result.valid = result.errors.empty();
        return result;
    }

private:
    [[nodiscard]] static bool is_semver(const std::string& version) {
        static const std::regex semver(
            R"(^([0-9]+)\.([0-9]+)\.([0-9]+)(?:-[0-9A-Za-z\.-]+)?(?:\+[0-9A-Za-z\.-]+)?$)");
        return std::regex_match(version, semver);
    }
};

} // namespace evoclaw::umi
