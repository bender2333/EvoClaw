#include "evolution/evolver.hpp"
#include "governance/governance_kernel.hpp"
#include "memory/org_log.hpp"
#include "llm/llm_client.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <cstdlib>
#include <string>

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


TEST_F(EvolverTest, MonitorAddsEwmaMetadata) {
    evoclaw::memory::OrgLog log(test_dir_);

    const std::vector<double> scores = {0.95, 0.9, 0.88, 0.55, 0.42, 0.35};
    for (double score : scores) {
        evoclaw::memory::OrgLogEntry e;
        e.agent_id = "agent-ewma";
        e.critic_score = score;
        log.append(e);
    }

    evoclaw::evolution::Evolver::Config config;
    config.kpi_decline_threshold = 0.75;
    config.ewma_decay = 0.3;
    config.volatility_sensitivity = 0.25;
    evoclaw::evolution::Evolver evolver(*kernel_, config);

    const auto tensions = evolver.monitor(log);
    bool found = false;
    for (const auto& tension : tensions) {
        if (tension.type == evoclaw::evolution::TensionType::KPI_DECLINE &&
            tension.source_agent == "agent-ewma") {
            found = true;
            EXPECT_TRUE(tension.metadata.contains("ewma_score"));
            EXPECT_TRUE(tension.metadata.contains("ewma_volatility"));
            EXPECT_TRUE(tension.metadata.contains("adaptive_threshold"));
            EXPECT_GT(tension.metadata.value("adaptive_threshold", 0.0), 0.0);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(EvolverTest, ProposeHasDefaultPatchValue) {
    evoclaw::evolution::Evolver evolver(*kernel_);

    evoclaw::evolution::Tension t;
    t.id = "tension-default";
    t.type = evoclaw::evolution::TensionType::KPI_DECLINE;
    t.source_agent = "agent-default";
    t.description = "score drop";
    t.severity = "high";

    const auto proposals = evolver.propose({t});
    ASSERT_EQ(proposals.size(), 1);
    EXPECT_TRUE(proposals[0].new_value.is_object());
    EXPECT_TRUE(proposals[0].new_value.contains("action"));
}

TEST_F(EvolverTest, ProposeUsesLlmWhenLiveEnabled) {
    const char* live_flag = std::getenv("EVOCLAW_LIVE_TEST");
    if (live_flag == nullptr || std::string(live_flag) != "1") {
        GTEST_SKIP() << "Set EVOCLAW_LIVE_TEST=1 to run live LLM proposal generation test.";
    }

    setenv("EVOCLAW_PROVIDER", "bailian", 1);
    auto llm_client = std::make_shared<evoclaw::llm::LLMClient>(evoclaw::llm::create_from_env());
    if (!llm_client || llm_client->is_mock_mode()) {
        GTEST_SKIP() << "No live Bailian config available.";
    }

    evoclaw::evolution::Evolver evolver(*kernel_);
    evolver.set_llm_client(llm_client);

    evoclaw::evolution::Tension t;
    t.id = "tension-live";
    t.type = evoclaw::evolution::TensionType::KPI_DECLINE;
    t.source_agent = "agent-live";
    t.description = "quality has dropped for recent tasks";
    t.severity = "high";

    const auto proposals = evolver.propose({t});
    ASSERT_EQ(proposals.size(), 1);
    EXPECT_TRUE(proposals[0].new_value.is_object());
    EXPECT_TRUE(proposals[0].new_value.contains("generated_by"));
    EXPECT_EQ(proposals[0].new_value.value("generated_by", std::string()), "llm");
}

} // namespace
