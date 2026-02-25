#include "governance/governance_kernel.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>

namespace evoclaw::governance {
namespace {

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool is_high_impact_action(const std::string& action) {
    const std::string lowered = to_lower(action);
    return lowered.find("deploy") != std::string::npos ||
           lowered.find("delete") != std::string::npos ||
           lowered.find("replace") != std::string::npos ||
           lowered.find("migrate") != std::string::npos;
}

} // namespace

GovernanceKernel::GovernanceKernel(Constitution constitution)
    : constitution_(std::move(constitution)) {}

Permission GovernanceKernel::evaluate_action(const std::string& actor,
                                             const std::string& action,
                                             const nlohmann::json& details) const {
    const nlohmann::json payload = {
        {"actor", actor},
        {"description", action},
        {"details", details}
    };

    if (constitution_.check_violation(payload)) {
        return Permission::DENY;
    }

    const bool requires_confirmation = (details.is_object() && details.value("requires_confirmation", false)) ||
                                       (details.is_object() && details.value("high_impact", false)) ||
                                       is_high_impact_action(action);
    if (requires_confirmation) {
        return Permission::REQUIRE_CONFIRMATION;
    }

    return Permission::ALLOW;
}

bool GovernanceKernel::allow_deployment(const umi::ModuleContract& contract,
                                        const std::vector<double>& ab_scores) const {
    if (!contract.validate() || ab_scores.empty()) {
        return false;
    }

    const double threshold = constitution_.evolution_policy.value("deployment_score_threshold", 0.8);
    for (const double score : ab_scores) {
        if (!std::isfinite(score) || score < threshold) {
            return false;
        }
    }
    return true;
}

bool GovernanceKernel::should_rollback(const AgentId& agent_id,
                                       const double recent_score,
                                       const double baseline_score) const {
    (void)agent_id;

    if (!std::isfinite(recent_score) || !std::isfinite(baseline_score) || baseline_score <= 0.0) {
        return false;
    }
    return recent_score < (baseline_score * 0.8);
}

} // namespace evoclaw::governance
