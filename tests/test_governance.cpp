#include "governance/constitution.hpp"
#include "governance/governance_kernel.hpp"

#include <gtest/gtest.h>

namespace {

evoclaw::governance::Constitution make_constitution() {
    evoclaw::governance::Constitution c;
    c.mission = "Build safe AI systems";
    c.values = {"safety", "transparency", "efficiency"};
    c.guardrails = {"delete production", "bypass security", "exfiltrate"};
    c.evolution_policy = {{"deployment_score_threshold", 0.8}};
    return c;
}

TEST(ConstitutionTest, CheckViolationDetectsKeyword) {
    auto c = make_constitution();

    EXPECT_TRUE(c.check_violation("delete production database"));
    EXPECT_TRUE(c.check_violation(nlohmann::json{{"description", "bypass security checks"}}));
    EXPECT_TRUE(c.check_violation("Exfiltrate user data"));  // case insensitive
}

TEST(ConstitutionTest, CheckViolationAllowsSafeActions) {
    auto c = make_constitution();

    EXPECT_FALSE(c.check_violation("update agent prompt"));
    EXPECT_FALSE(c.check_violation(nlohmann::json{{"description", "run benchmark"}}));
}

TEST(ConstitutionTest, EmptyGuardrailsNeverViolate) {
    evoclaw::governance::Constitution c;
    EXPECT_FALSE(c.check_violation("anything goes"));
}

TEST(GovernanceKernelTest, EvaluateActionDeniesViolation) {
    evoclaw::governance::GovernanceKernel kernel(make_constitution());

    auto perm = kernel.evaluate_action("agent-1", "delete production data", {});
    EXPECT_EQ(perm, evoclaw::governance::Permission::DENY);
}

TEST(GovernanceKernelTest, EvaluateActionAllowsSafe) {
    evoclaw::governance::GovernanceKernel kernel(make_constitution());

    auto perm = kernel.evaluate_action("agent-1", "update prompt", {});
    EXPECT_EQ(perm, evoclaw::governance::Permission::ALLOW);
}

TEST(GovernanceKernelTest, HighImpactRequiresConfirmation) {
    evoclaw::governance::GovernanceKernel kernel(make_constitution());

    auto perm = kernel.evaluate_action("agent-1", "deploy new version", {});
    EXPECT_EQ(perm, evoclaw::governance::Permission::REQUIRE_CONFIRMATION);
}

TEST(GovernanceKernelTest, AllowDeploymentChecksScores) {
    evoclaw::governance::GovernanceKernel kernel(make_constitution());

    evoclaw::umi::ModuleContract contract;
    contract.module_id = "test.mod";
    contract.version = "1.0.0";
    contract.capability.intent_tags = {"test"};
    contract.capability.success_rate_threshold = 0.75;
    contract.capability.estimated_cost_token = 100.0;
    contract.airbag.level = evoclaw::umi::AirbagLevel::BASIC;
    contract.airbag.reversible = true;
    contract.cost_model = {{"tokens", 100.0}};

    // 高分通过
    EXPECT_TRUE(kernel.allow_deployment(contract, {0.9, 0.85, 0.95}));

    // 低分拒绝
    EXPECT_FALSE(kernel.allow_deployment(contract, {0.9, 0.5, 0.95}));

    // 空分数拒绝
    EXPECT_FALSE(kernel.allow_deployment(contract, {}));
}

TEST(GovernanceKernelTest, ShouldRollback) {
    evoclaw::governance::GovernanceKernel kernel(make_constitution());

    // 最近分数低于基线 80% → 回滚
    EXPECT_TRUE(kernel.should_rollback("agent-1", 0.5, 0.9));

    // 最近分数正常 → 不回滚
    EXPECT_FALSE(kernel.should_rollback("agent-1", 0.8, 0.9));

    // 基线为 0 → 不回滚
    EXPECT_FALSE(kernel.should_rollback("agent-1", 0.5, 0.0));
}

} // namespace
