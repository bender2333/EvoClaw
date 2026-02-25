#pragma once

#include "core/types.hpp"
#include "governance/constitution.hpp"
#include "umi/contract.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace evoclaw::governance {

enum class Permission {
    ALLOW,
    DENY,
    REQUIRE_CONFIRMATION
};

class GovernanceKernel {
public:
    explicit GovernanceKernel(Constitution constitution);

    [[nodiscard]] Permission evaluate_action(const std::string& actor,
                                             const std::string& action,
                                             const nlohmann::json& details) const;

    [[nodiscard]] bool allow_deployment(const umi::ModuleContract& contract,
                                        const std::vector<double>& ab_scores) const;

    [[nodiscard]] bool should_rollback(const AgentId& agent_id,
                                       double recent_score,
                                       double baseline_score) const;

private:
    Constitution constitution_;
};

} // namespace evoclaw::governance
