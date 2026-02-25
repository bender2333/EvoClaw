#include "evolution/evolver.hpp"
#include "governance/governance_kernel.hpp"
#include "memory/org_log.hpp"

#include <gtest/gtest.h>
#include <filesystem>

namespace {

class EvolverTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_evo_" + evoclaw::generate_uuid());
        std::filesystem::create_directories(test_dir_);

        evoclaw::governance::Constitution c;
        c.mission = "Test";
        c.values = {"quality"};
        c.guardrails = {};
        c.evolution_policy = {{"deployment_score_threshold", 0.8}};
        kernel_ = std::make_unique<evoclaw::governance::GovernanceKernel>(c);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::filesystem::path test_dir_;
    std::unique_ptr<evoclaw::governance::GovernanceKernel> kernel_;
};

TEST_F(EvolverTest, MonitorDetectsKPIDecline) {
    evoclaw::memory::OrgLog log(test_dir_);

    // 前 10 个高分
    for (int i = 0; i < 10; ++i) {
        evoclaw::memory::OrgLogEntry e;
        e.agent_id = "agent-a";
        e.critic_score = 0.9;
        log.append(e);
    }
    // 后 5 个低分
    for (int i = 0; i < 5; ++i) {
        evoclaw::memory::OrgLogEntry e;
        e.agent_id = "agent-a";
        e.critic_score = 0.3;
        log.append(e);
    }

    evoclaw::evolution::Evolver::Config config;
    config.kpi_decline_threshold = 0.8;
    evoclaw::evolution::Evolver evolver(*kernel_, config);

    auto tensions = evolver.monitor(log);
    EXPECT_FALSE(tensions.empty());

    bool found_kpi = false;
    for (const auto& t : tensions) {
        if (t.type == evoclaw::evolution::TensionType::KPI_DECLINE) {
            found_kpi = true;
            EXPECT_EQ(t.source_agent, "agent-a");
            EXPECT_EQ(t.severity, "high");
        }
    }
    EXPECT_TRUE(found_kpi);
}

TEST_F(EvolverTest, MonitorDetectsRepeatedFailure) {
    evoclaw::memory::OrgLog log(test_dir_);

    evoclaw::evolution::Evolver::Config config;
    config.consecutive_failures = 5;
    evoclaw::evolution::Evolver evolver(*kernel_, config);

    // 10 个低分（< 0.5 算失败）
    for (int i = 0; i < 10; ++i) {
        evoclaw::memory::OrgLogEntry e;
        e.agent_id = "agent-b";
        e.critic_score = 0.2;
        log.append(e);
    }

    auto tensions = evolver.monitor(log);
    bool found_failure = false;
    for (const auto& t : tensions) {
        if (t.type == evoclaw::evolution::TensionType::REPEATED_FAILURE) {
            found_failure = true;
            EXPECT_EQ(t.source_agent, "agent-b");
        }
    }
    EXPECT_TRUE(found_failure);
}

TEST_F(EvolverTest, ProposeGeneratesProposals) {
    evoclaw::evolution::Evolver evolver(*kernel_);

    evoclaw::evolution::Tension t;
    t.id = "tension-1";
    t.type = evoclaw::evolution::TensionType::KPI_DECLINE;
    t.source_agent = "agent-a";
    t.description = "KPI decline";
    t.severity = "high";

    auto proposals = evolver.propose({t});
    EXPECT_EQ(proposals.size(), 1);
    EXPECT_EQ(proposals[0].target_agent, "agent-a");
    EXPECT_EQ(proposals[0].type, evoclaw::evolution::EvolutionType::ADJUSTMENT);
    EXPECT_EQ(proposals[0].tension_ids.size(), 1);
    EXPECT_EQ(proposals[0].tension_ids[0], "tension-1");
}

TEST_F(EvolverTest, ABTestSignificantImprovement) {
    evoclaw::evolution::Evolver evolver(*kernel_);

    evoclaw::evolution::EvolutionProposal proposal;
    proposal.id = "p1";
    proposal.target_agent = "agent-a";

    std::vector<double> control = {0.5, 0.5, 0.5, 0.5, 0.5};
    std::vector<double> candidate = {0.8, 0.8, 0.8, 0.8, 0.8};

    auto result = evolver.run_ab_test(proposal, control, candidate);
    EXPECT_TRUE(result.significant);
    EXPECT_GT(result.improvement, 0.05);
    EXPECT_NEAR(result.control_score, 0.5, 0.01);
    EXPECT_NEAR(result.candidate_score, 0.8, 0.01);
}

TEST_F(EvolverTest, ABTestInsignificantImprovement) {
    evoclaw::evolution::Evolver evolver(*kernel_);

    evoclaw::evolution::EvolutionProposal proposal;
    proposal.id = "p2";
    proposal.target_agent = "agent-a";

    std::vector<double> control = {0.8, 0.8, 0.8, 0.8, 0.8};
    std::vector<double> candidate = {0.81, 0.81, 0.81, 0.81, 0.81};

    auto result = evolver.run_ab_test(proposal, control, candidate);
    EXPECT_FALSE(result.significant);
}

TEST_F(EvolverTest, ApplyEvolutionRequiresSignificance) {
    evoclaw::evolution::Evolver evolver(*kernel_);

    evoclaw::evolution::EvolutionProposal proposal;
    proposal.id = "p3";
    proposal.target_agent = "agent-a";
    proposal.description = "adjust prompt";

    evoclaw::evolution::ABTestResult insignificant;
    insignificant.significant = false;
    insignificant.improvement = 0.01;

    EXPECT_FALSE(evolver.apply_evolution(proposal, insignificant));

    evoclaw::evolution::ABTestResult significant;
    significant.significant = true;
    significant.improvement = 0.2;

    EXPECT_TRUE(evolver.apply_evolution(proposal, significant));
}

} // namespace
