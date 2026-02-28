#include "facade/facade.hpp"
#include "agent/planner.hpp"
#include "agent/executor.hpp"
#include "agent/critic.hpp"

#include <gtest/gtest.h>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <string>

namespace {

evoclaw::agent::AgentConfig make_agent_config(const std::string& id,
                                               const std::string& role,
                                               const std::vector<std::string>& intents) {
    evoclaw::agent::AgentConfig config;
    config.id = id;
    config.role = role;
    config.system_prompt = role + " agent";
    config.contract.module_id = id;
    config.contract.version = "1.0.0";
    config.contract.capability.intent_tags = intents;
    config.contract.capability.success_rate_threshold = 0.75;
    config.contract.capability.estimated_cost_token = 100.0;
    config.contract.airbag.level = evoclaw::umi::AirbagLevel::BASIC;
    config.contract.airbag.reversible = true;
    config.contract.cost_model = {{"tokens", 100.0}};
    return config;
}

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 确保集成测试不调用真实 LLM
        saved_key_ = getenv("EVOCLAW_API_KEY") ? std::string(getenv("EVOCLAW_API_KEY")) : "";
        saved_home_ = getenv("HOME") ? std::string(getenv("HOME")) : "";
        unsetenv("EVOCLAW_API_KEY");
        
        test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_int_" + evoclaw::generate_uuid());
        std::filesystem::create_directories(test_dir_);
        
        // 设置假 HOME 防止读取真实 openclaw.json
        setenv("HOME", test_dir_.string().c_str(), 1);
    }

    void TearDown() override {
        if (!saved_key_.empty()) {
            setenv("EVOCLAW_API_KEY", saved_key_.c_str(), 1);
        }
        if (!saved_home_.empty()) {
            setenv("HOME", saved_home_.c_str(), 1);
        }
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::filesystem::path test_dir_;
    std::string saved_key_;
    std::string saved_home_;
};

TEST_F(IntegrationTest, FullSystemWorkflow) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_agent_config("planner-1", "planner", {"plan"}));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_agent_config("executor-1", "executor", {"execute"}));

    facade.register_agent(planner);
    facade.register_agent(executor);

    // 提交任务
    evoclaw::agent::Task task;
    task.id = "task-1";
    task.description = "Test task";
    task.type = evoclaw::TaskType::EXECUTE;

    auto result = facade.submit_task(task);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.output.empty());

    // 多次任务
    for (int i = 0; i < 10; ++i) {
        task.id = "task-batch-" + std::to_string(i);
        auto batch_result = facade.submit_task(task);
        (void)batch_result;
    }

    // 验证状态
    auto status = facade.get_status();
    EXPECT_TRUE(status.contains("agents") || status.contains("agent_count"));

    // 触发进化
    facade.trigger_evolution();

    // 验证事件日志完整性
    EXPECT_TRUE(facade.verify_event_log());
}


TEST_F(IntegrationTest, ExecutePipelineAddsStructuredMetadata) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_agent_config("planner-1", "planner", {"plan"}));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_agent_config("executor-1", "executor", {"execute"}));
    auto critic = std::make_shared<evoclaw::agent::Critic>(
        make_agent_config("critic-1", "critic", {"critique"}));

    facade.register_agent(planner);
    facade.register_agent(executor);
    facade.register_agent(critic);

    evoclaw::agent::Task task;
    task.id = "task-pipeline";
    task.description = "Summarize one sentence";
    task.type = evoclaw::TaskType::EXECUTE;
    task.budget.max_tokens = 300;
    task.budget.max_rounds = 3;
    task.budget.max_tool_calls = 3;

    const auto result = facade.submit_task(task);
    EXPECT_TRUE(result.metadata.contains("pipeline"));
    EXPECT_TRUE(result.metadata["pipeline"].value("enabled", false));
    EXPECT_EQ(result.metadata["pipeline"].value("stage_count", 0), 3);
    ASSERT_TRUE(result.metadata["pipeline"].contains("stages"));
    EXPECT_EQ(result.metadata["pipeline"]["stages"].size(), 3);
    EXPECT_TRUE(result.metadata.contains("critic"));

    const auto logs = facade.query_logs(
        evoclaw::now() - std::chrono::hours(1),
        evoclaw::now() + std::chrono::hours(1));
    ASSERT_FALSE(logs.empty());
    EXPECT_TRUE(logs.back().metadata.contains("result_metadata"));
}

TEST_F(IntegrationTest, PipelineRespectsRoundBudget) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_agent_config("planner-1", "planner", {"plan"}));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_agent_config("executor-1", "executor", {"execute"}));
    auto critic = std::make_shared<evoclaw::agent::Critic>(
        make_agent_config("critic-1", "critic", {"critique"}));

    facade.register_agent(planner);
    facade.register_agent(executor);
    facade.register_agent(critic);

    evoclaw::agent::Task task;
    task.id = "task-budget";
    task.description = "Budget bound test";
    task.type = evoclaw::TaskType::EXECUTE;
    task.budget.max_rounds = 2;
    task.budget.max_tool_calls = 3;

    const auto result = facade.submit_task(task);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.metadata.value("reason", std::string()), "budget_rounds_exceeded");
}



TEST_F(IntegrationTest, EvolutionAppliesRuntimePatchToAgentConfig) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    config.evolver_config.kpi_decline_threshold = 0.95;
    config.evolver_config.min_sample_size = 8;
    config.evolver_config.confidence_threshold = 0.7;
    config.evolver_config.max_proposals_per_cycle = 3;
    config.evolver_config.consecutive_failures = 100;
    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();

    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_agent_config("executor-1", "executor", {"execute"}));
    facade.register_agent(executor);

    const auto before = facade.get_agent_runtime_config("executor-1");
    ASSERT_TRUE(before.contains("system_prompt"));

    for (int i = 0; i < 10; ++i) {
        evoclaw::agent::Task task;
        task.id = "evo-kpi-" + std::to_string(i);
        task.description = "score-seeded route task";
        task.type = evoclaw::TaskType::ROUTE;
        task.context["intent"] = "execute";
        task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
        task.context["score"] = (i < 6) ? 0.9 : 0.8;

        const auto result = facade.submit_task(task);
        EXPECT_TRUE(result.success);
    }

    facade.trigger_evolution();

    const auto after = facade.get_agent_runtime_config("executor-1");
    ASSERT_TRUE(after.contains("system_prompt"));

    const std::string before_prompt = before["system_prompt"].get<std::string>();
    const std::string after_prompt = after["system_prompt"].get<std::string>();

    EXPECT_NE(after_prompt, before_prompt);
    EXPECT_NE(after_prompt.find("Prioritize accuracy"), std::string::npos);

    const auto status = facade.get_status();
    ASSERT_TRUE(status.contains("evolution"));
    const auto cycle = status["evolution"].value("last_cycle", nlohmann::json::object());
    ASSERT_TRUE(cycle.contains("proposals"));

    bool saw_runtime_patch = false;
    for (const auto& proposal : cycle["proposals"]) {
        if (proposal.value("applied", false) && proposal.value("runtime_patch_applied", false)) {
            saw_runtime_patch = true;
            break;
        }
    }
    EXPECT_TRUE(saw_runtime_patch);
}


TEST_F(IntegrationTest, RollbackSnapshotStoresAndRestoresConfig) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    config.evolver_config.kpi_decline_threshold = 0.95;
    config.evolver_config.min_sample_size = 8;
    config.evolver_config.confidence_threshold = 0.7;
    config.evolver_config.max_proposals_per_cycle = 3;
    config.evolver_config.consecutive_failures = 100;
    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();

    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_agent_config("executor-rollback", "executor", {"execute"}));
    facade.register_agent(executor);

    const auto before = facade.get_agent_runtime_config("executor-rollback");

    for (int i = 0; i < 10; ++i) {
        evoclaw::agent::Task task;
        task.id = "rollback-kpi-" + std::to_string(i);
        task.description = "score task";
        task.type = evoclaw::TaskType::ROUTE;
        task.context["intent"] = "execute";
        task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
        task.context["score"] = (i < 6) ? 0.9 : 0.8;
        facade.submit_task(task);
    }

    facade.trigger_evolution();

    const auto status = facade.get_status();
    const auto cycle = status["evolution"].value("last_cycle", nlohmann::json::object());
    ASSERT_TRUE(cycle.contains("proposals"));

    std::string applied_proposal_id;
    for (const auto& proposal : cycle["proposals"]) {
        if (proposal.value("applied", false) && proposal.value("runtime_patch_applied", false)) {
            applied_proposal_id = proposal["id"].get<std::string>();
            break;
        }
    }
    ASSERT_FALSE(applied_proposal_id.empty());

    const auto snapshots = facade.list_rollback_snapshots();
    bool found = false;
    for (const auto& snap : snapshots) {
        if (snap["proposal_id"] == applied_proposal_id) {
            found = true;
            EXPECT_EQ(snap["agent_id"].get<std::string>(), "executor-rollback");
            EXPECT_TRUE(snap.contains("config_before"));
            EXPECT_TRUE(snap.contains("config_after"));
            break;
        }
    }
    EXPECT_TRUE(found);

    const auto after = facade.get_agent_runtime_config("executor-rollback");
    EXPECT_NE(after["system_prompt"], before["system_prompt"]);

    std::string reason;
    const bool rolled_back = facade.rollback_proposal(applied_proposal_id, &reason);
    EXPECT_TRUE(rolled_back) << reason;

    const auto restored = facade.get_agent_runtime_config("executor-rollback");
    EXPECT_EQ(restored["system_prompt"], before["system_prompt"]);
}


TEST_F(IntegrationTest, ValidatePatchSchemaRejectsUnknownFields) {
    std::string reason;
    EXPECT_TRUE(evoclaw::facade::EvoClawFacade::validate_patch_schema(
        {{"temperature", 0.5}, {"system_prompt_suffix", "be careful"}}, &reason));

    EXPECT_FALSE(evoclaw::facade::EvoClawFacade::validate_patch_schema(
        {{"temperature", 0.5}, {"malicious_field", "drop table"}}, &reason));
    EXPECT_NE(reason.find("unknown field"), std::string::npos);

    EXPECT_FALSE(evoclaw::facade::EvoClawFacade::validate_patch_schema(
        {{"temperature", "not_a_number"}}, &reason));
    EXPECT_NE(reason.find("must be a number"), std::string::npos);

    EXPECT_TRUE(evoclaw::facade::EvoClawFacade::validate_patch_schema(
        {{"patch", {{"temperature", 0.7}}}}, &reason));

    EXPECT_FALSE(evoclaw::facade::EvoClawFacade::validate_patch_schema(
        {{"patch", {{"evil", true}}}}, &reason));
    EXPECT_NE(reason.find("unknown field"), std::string::npos);
}

TEST(IntegrationLiveTest, ExecutePipelineWithBailianWhenEnabled) {
    const char* live_flag = std::getenv("EVOCLAW_LIVE_TEST");
    if (live_flag == nullptr || std::string(live_flag) != "1") {
        GTEST_SKIP() << "Set EVOCLAW_LIVE_TEST=1 to run live end-to-end pipeline test.";
    }

    setenv("EVOCLAW_PROVIDER", "bailian", 1);

    const auto temp_dir = std::filesystem::temp_directory_path() /
                          ("evoclaw_live_int_" + evoclaw::generate_uuid());
    std::filesystem::create_directories(temp_dir);

    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = temp_dir;
    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();

    auto status = facade.get_status();
    if (status.contains("llm") && status["llm"].is_object() &&
        status["llm"].value("mock_mode", true)) {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
        GTEST_SKIP() << "LLM client is in mock mode; Bailian config unavailable.";
    }

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_agent_config("planner-live", "planner", {"plan"}));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_agent_config("executor-live", "executor", {"execute"}));
    auto critic = std::make_shared<evoclaw::agent::Critic>(
        make_agent_config("critic-live", "critic", {"critique"}));

    facade.register_agent(planner);
    facade.register_agent(executor);
    facade.register_agent(critic);

    evoclaw::agent::Task task;
    task.id = "live-pipeline-task";
    task.description = "List three benefits of code review in one short paragraph.";
    task.type = evoclaw::TaskType::EXECUTE;
    task.budget.max_tokens = 768;
    task.budget.max_rounds = 3;
    task.budget.max_tool_calls = 3;

    const auto result = facade.submit_task(task);
    EXPECT_TRUE(result.metadata.contains("pipeline"));
    EXPECT_TRUE(result.metadata["pipeline"].value("enabled", false));
    ASSERT_TRUE(result.metadata["pipeline"].contains("stages"));
    EXPECT_EQ(result.metadata["pipeline"]["stages"].size(), 3);

    const auto logs = facade.query_logs(
        evoclaw::now() - std::chrono::hours(1),
        evoclaw::now() + std::chrono::hours(1));
    ASSERT_FALSE(logs.empty());
    EXPECT_TRUE(logs.back().metadata.contains("result_metadata"));

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

} // namespace
