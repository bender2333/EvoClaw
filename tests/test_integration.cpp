#include "facade/facade.hpp"
#include "agent/planner.hpp"
#include "agent/executor.hpp"
#include "agent/critic.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
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

nlohmann::json make_runtime_snapshot(const std::string& id,
                                     const std::string& role,
                                     const std::string& prompt,
                                     const std::string& module_version) {
    return {
        {"id", id},
        {"role", role},
        {"system_prompt", prompt},
        {"temperature", 0.7},
        {"contract", {
            {"module_id", id},
            {"version", module_version},
            {"success_rate_threshold", 0.75},
            {"estimated_cost_token", 100.0},
            {"intent_tags", nlohmann::json::array({"execute"})},
            {"required_tools", nlohmann::json::array()}
        }}
    };
}

void write_json_file(const std::filesystem::path& path, const nlohmann::json& data) {
    std::ofstream ofs(path);
    ASSERT_TRUE(ofs.is_open());
    ofs << data.dump(2);
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

TEST_F(IntegrationTest, RuntimeConfigVersioningTracksPatchRollbackAndDiff) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    config.evolver_config.kpi_decline_threshold = 0.95;
    config.evolver_config.min_sample_size = 8;
    config.evolver_config.confidence_threshold = 0.7;
    config.evolver_config.max_proposals_per_cycle = 1;
    config.evolver_config.consecutive_failures = 100;
    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();

    const std::string agent_id = "executor-versioning";
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_agent_config(agent_id, "executor", {"execute"}));
    facade.register_agent(executor);

    const auto summary_for = [&agent_id](const nlohmann::json& runtime_config) -> nlohmann::json {
        if (!runtime_config.contains("agents") || !runtime_config["agents"].is_array()) {
            return nlohmann::json();
        }
        for (const auto& item : runtime_config["agents"]) {
            if (item.value("agent_id", std::string()) == agent_id) {
                return item;
            }
        }
        return nlohmann::json();
    };

    const auto version0 = facade.get_agent_runtime_version(agent_id);
    ASSERT_TRUE(version0.contains("version"));
    EXPECT_EQ(version0["version"].get<std::uint64_t>(), 0U);

    const auto status0 = facade.get_status();
    ASSERT_TRUE(status0.contains("runtime_config"));
    const auto runtime0 = status0["runtime_config"];
    EXPECT_EQ(runtime0["tracked_agents"].get<std::size_t>(), 1U);
    EXPECT_EQ(runtime0["history_entries"].get<std::size_t>(), 0U);
    const auto runtime0_agent = summary_for(runtime0);
    ASSERT_TRUE(runtime0_agent.is_object());
    EXPECT_EQ(runtime0_agent["current_version"].get<std::uint64_t>(), 0U);
    EXPECT_EQ(runtime0_agent["history_count"].get<std::size_t>(), 0U);
    EXPECT_TRUE(runtime0_agent["latest_changed_at"].is_null());
    EXPECT_FALSE(runtime0_agent.contains("before"));
    EXPECT_FALSE(runtime0_agent.contains("after"));
    EXPECT_FALSE(runtime0_agent.contains("diff"));

    for (int i = 0; i < 10; ++i) {
        evoclaw::agent::Task task;
        task.id = "version-kpi-" + std::to_string(i);
        task.description = "score task";
        task.type = evoclaw::TaskType::ROUTE;
        task.context["intent"] = "execute";
        task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
        task.context["score"] = (i < 6) ? 0.9 : 0.8;
        (void)facade.submit_task(task);
    }

    facade.trigger_evolution();

    const auto status = facade.get_status();
    ASSERT_TRUE(status.contains("runtime_config"));
    const auto runtime1 = status["runtime_config"];
    EXPECT_EQ(runtime1["tracked_agents"].get<std::size_t>(), 1U);
    EXPECT_EQ(runtime1["history_entries"].get<std::size_t>(), 1U);
    const auto runtime1_agent = summary_for(runtime1);
    ASSERT_TRUE(runtime1_agent.is_object());
    EXPECT_EQ(runtime1_agent["current_version"].get<std::uint64_t>(), 1U);
    EXPECT_EQ(runtime1_agent["history_count"].get<std::size_t>(), 1U);
    EXPECT_TRUE(runtime1_agent["latest_changed_at"].is_string());
    EXPECT_FALSE(runtime1_agent.contains("before"));
    EXPECT_FALSE(runtime1_agent.contains("after"));
    EXPECT_FALSE(runtime1_agent.contains("diff"));

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

    const auto version1 = facade.get_agent_runtime_version(agent_id);
    ASSERT_TRUE(version1.contains("version"));
    EXPECT_EQ(version1["version"].get<std::uint64_t>(), 1U);

    const auto history1 = facade.get_agent_runtime_history(agent_id);
    ASSERT_TRUE(history1.is_array());
    ASSERT_EQ(history1.size(), 1U);
    EXPECT_EQ(history1[0]["version"].get<std::uint64_t>(), 1U);
    EXPECT_EQ(history1[0]["proposal_id"].get<std::string>(), applied_proposal_id);
    EXPECT_TRUE(history1[0].contains("diff"));
    EXPECT_FALSE(history1[0]["diff"].empty());

    const auto diff01 = facade.get_agent_runtime_diff(agent_id, 0, 1);
    ASSERT_FALSE(diff01.contains("error"));
    ASSERT_TRUE(diff01.contains("diff"));
    EXPECT_TRUE(diff01["diff"].contains("system_prompt"));

    std::string reason;
    const bool rolled_back = facade.rollback_proposal(applied_proposal_id, &reason);
    EXPECT_TRUE(rolled_back) << reason;

    const auto version2 = facade.get_agent_runtime_version(agent_id);
    ASSERT_TRUE(version2.contains("version"));
    EXPECT_EQ(version2["version"].get<std::uint64_t>(), 2U);

    const auto status2 = facade.get_status();
    ASSERT_TRUE(status2.contains("runtime_config"));
    const auto runtime2 = status2["runtime_config"];
    EXPECT_EQ(runtime2["tracked_agents"].get<std::size_t>(), 1U);
    EXPECT_EQ(runtime2["history_entries"].get<std::size_t>(), 2U);
    const auto runtime2_agent = summary_for(runtime2);
    ASSERT_TRUE(runtime2_agent.is_object());
    EXPECT_EQ(runtime2_agent["current_version"].get<std::uint64_t>(), 2U);
    EXPECT_EQ(runtime2_agent["history_count"].get<std::size_t>(), 2U);
    EXPECT_TRUE(runtime2_agent["latest_changed_at"].is_string());

    const auto history2 = facade.get_agent_runtime_history(agent_id);
    ASSERT_TRUE(history2.is_array());
    ASSERT_EQ(history2.size(), 2U);
    EXPECT_EQ(history2.back()["version"].get<std::uint64_t>(), 2U);

    const auto diff12 = facade.get_agent_runtime_diff(agent_id, 1, 2);
    ASSERT_FALSE(diff12.contains("error"));
    EXPECT_TRUE(diff12["diff"].contains("system_prompt"));

    const auto diff02 = facade.get_agent_runtime_diff(agent_id, 0, 2);
    ASSERT_FALSE(diff02.contains("error"));
    EXPECT_TRUE(diff02["diff"].empty());
}

TEST_F(IntegrationTest, RuntimeConfigVersionHistoryPersistenceRoundTrip) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    config.evolver_config.kpi_decline_threshold = 0.95;
    config.evolver_config.min_sample_size = 8;
    config.evolver_config.confidence_threshold = 0.7;
    config.evolver_config.max_proposals_per_cycle = 1;
    config.evolver_config.consecutive_failures = 100;

    const std::string agent_id = "executor-version-persist";
    std::uint64_t version_before = 0U;
    nlohmann::json history_before = nlohmann::json::array();

    {
        evoclaw::facade::EvoClawFacade facade(config);
        facade.initialize();

        auto executor = std::make_shared<evoclaw::agent::Executor>(
            make_agent_config(agent_id, "executor", {"execute"}));
        facade.register_agent(executor);

        for (int i = 0; i < 10; ++i) {
            evoclaw::agent::Task task;
            task.id = "persist-version-kpi-" + std::to_string(i);
            task.description = "score task";
            task.type = evoclaw::TaskType::ROUTE;
            task.context["intent"] = "execute";
            task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
            task.context["score"] = (i < 6) ? 0.9 : 0.8;
            (void)facade.submit_task(task);
        }

        facade.trigger_evolution();

        const auto status = facade.get_status();
        const auto cycle = status["evolution"].value("last_cycle", nlohmann::json::object());
        ASSERT_TRUE(cycle.contains("proposals"));

        bool saw_runtime_patch = false;
        for (const auto& proposal : cycle["proposals"]) {
            if (proposal.value("applied", false) && proposal.value("runtime_patch_applied", false)) {
                saw_runtime_patch = true;
                break;
            }
        }
        ASSERT_TRUE(saw_runtime_patch);

        const auto version_json = facade.get_agent_runtime_version(agent_id);
        ASSERT_TRUE(version_json.contains("version"));
        version_before = version_json["version"].get<std::uint64_t>();
        ASSERT_GT(version_before, 0U);

        history_before = facade.get_agent_runtime_history(agent_id);
        ASSERT_TRUE(history_before.is_array());
        ASSERT_FALSE(history_before.empty());
        EXPECT_EQ(history_before.back()["version"].get<std::uint64_t>(), version_before);

        facade.save_state();
    }

    EXPECT_TRUE(std::filesystem::exists(test_dir_ / "runtime_config_versions.json"));
    EXPECT_TRUE(std::filesystem::exists(test_dir_ / "runtime_config_history.json"));

    {
        evoclaw::facade::EvoClawFacade facade2(config);
        facade2.initialize();

        auto executor2 = std::make_shared<evoclaw::agent::Executor>(
            make_agent_config(agent_id, "executor", {"execute"}));
        facade2.register_agent(executor2);

        const auto version_after_json = facade2.get_agent_runtime_version(agent_id);
        ASSERT_TRUE(version_after_json.contains("version"));
        EXPECT_EQ(version_after_json["version"].get<std::uint64_t>(), version_before);

        const auto history_after = facade2.get_agent_runtime_history(agent_id);
        ASSERT_TRUE(history_after.is_array());
        ASSERT_EQ(history_after.size(), history_before.size());
        EXPECT_EQ(history_after, history_before);
    }
}

TEST_F(IntegrationTest, RuntimeConfigHistoryPruneKeepsRecentPerAgentAndDiffFailsForPrunedVersions) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;

    const std::string agent_a = "executor-runtime-prune-a";
    const std::string agent_b = "executor-runtime-prune-b";

    const auto a0 = make_runtime_snapshot(agent_a, "executor", "executor A prompt v0", "1.0.0");
    const auto a1 = make_runtime_snapshot(agent_a, "executor", "executor A prompt v1", "1.0.1");
    const auto a2 = make_runtime_snapshot(agent_a, "executor", "executor A prompt v2", "1.0.2");
    const auto a3 = make_runtime_snapshot(agent_a, "executor", "executor A prompt v3", "1.0.3");
    const auto a4 = make_runtime_snapshot(agent_a, "executor", "executor A prompt v4", "1.0.4");

    const auto b0 = make_runtime_snapshot(agent_b, "executor", "executor B prompt v0", "1.0.0");
    const auto b1 = make_runtime_snapshot(agent_b, "executor", "executor B prompt v1", "1.0.1");
    const auto b2 = make_runtime_snapshot(agent_b, "executor", "executor B prompt v2", "1.0.2");

    nlohmann::json history = nlohmann::json::array();
    const auto push_record = [&history](const std::string& agent_id,
                                        const std::uint64_t version,
                                        const std::string& proposal_id,
                                        const nlohmann::json& before,
                                        const nlohmann::json& after,
                                        const std::string& changed_at) {
        history.push_back({
            {"agent_id", agent_id},
            {"version", version},
            {"proposal_id", proposal_id},
            {"before", before},
            {"after", after},
            {"diff", {{"system_prompt", {{"before", before["system_prompt"]}, {"after", after["system_prompt"]}}}}},
            {"changed_at", changed_at}
        });
    };

    push_record(agent_a, 1U, "proposal-a-1", a0, a1, "2026-03-01T10:00:00");
    push_record(agent_b, 1U, "proposal-b-1", b0, b1, "2026-03-01T10:01:00");
    push_record(agent_a, 2U, "proposal-a-2", a1, a2, "2026-03-01T10:02:00");
    push_record(agent_b, 2U, "proposal-b-2", b1, b2, "2026-03-01T10:03:00");
    push_record(agent_a, 3U, "proposal-a-3", a2, a3, "2026-03-01T10:04:00");
    push_record(agent_a, 4U, "proposal-a-4", a3, a4, "2026-03-01T10:05:00");

    write_json_file(test_dir_ / "runtime_config_versions.json", {
        {agent_a, 4U},
        {agent_b, 2U}
    });
    write_json_file(test_dir_ / "runtime_config_history.json", history);

    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();
    facade.register_agent(std::make_shared<evoclaw::agent::Executor>(
        make_agent_config(agent_a, "executor", {"execute"})));
    facade.register_agent(std::make_shared<evoclaw::agent::Executor>(
        make_agent_config(agent_b, "executor", {"execute"})));

    const auto version_a_before = facade.get_agent_runtime_version(agent_a);
    const auto version_b_before = facade.get_agent_runtime_version(agent_b);
    EXPECT_EQ(version_a_before["version"].get<std::uint64_t>(), 4U);
    EXPECT_EQ(version_b_before["version"].get<std::uint64_t>(), 2U);
    EXPECT_EQ(facade.get_agent_runtime_history(agent_a).size(), 4U);
    EXPECT_EQ(facade.get_agent_runtime_history(agent_b).size(), 2U);

    facade.clear_old_runtime_config_history(2);

    const auto version_a_after = facade.get_agent_runtime_version(agent_a);
    const auto version_b_after = facade.get_agent_runtime_version(agent_b);
    EXPECT_EQ(version_a_after["version"].get<std::uint64_t>(), 4U);
    EXPECT_EQ(version_b_after["version"].get<std::uint64_t>(), 2U);

    const auto history_a_after = facade.get_agent_runtime_history(agent_a);
    ASSERT_EQ(history_a_after.size(), 2U);
    EXPECT_EQ(history_a_after[0]["version"].get<std::uint64_t>(), 3U);
    EXPECT_EQ(history_a_after[1]["version"].get<std::uint64_t>(), 4U);

    const auto history_b_after = facade.get_agent_runtime_history(agent_b);
    ASSERT_EQ(history_b_after.size(), 2U);
    EXPECT_EQ(history_b_after[0]["version"].get<std::uint64_t>(), 1U);
    EXPECT_EQ(history_b_after[1]["version"].get<std::uint64_t>(), 2U);

    const auto status = facade.get_status();
    ASSERT_TRUE(status.contains("runtime_config"));
    const auto runtime = status["runtime_config"];
    EXPECT_EQ(runtime["history_entries"].get<std::size_t>(), 4U);

    const auto runtime_summary_for = [&runtime](const std::string& id) -> nlohmann::json {
        if (!runtime.contains("agents") || !runtime["agents"].is_array()) {
            return nlohmann::json();
        }
        for (const auto& item : runtime["agents"]) {
            if (item.value("agent_id", std::string()) == id) {
                return item;
            }
        }
        return nlohmann::json();
    };

    const auto summary_a = runtime_summary_for(agent_a);
    ASSERT_TRUE(summary_a.is_object());
    EXPECT_EQ(summary_a["current_version"].get<std::uint64_t>(), 4U);
    EXPECT_EQ(summary_a["history_count"].get<std::size_t>(), 2U);

    const auto summary_b = runtime_summary_for(agent_b);
    ASSERT_TRUE(summary_b.is_object());
    EXPECT_EQ(summary_b["current_version"].get<std::uint64_t>(), 2U);
    EXPECT_EQ(summary_b["history_count"].get<std::size_t>(), 2U);

    const auto pruned_diff = facade.get_agent_runtime_diff(agent_a, 1, 2);
    EXPECT_EQ(pruned_diff.value("error", std::string()), "version_not_found");
    EXPECT_EQ(pruned_diff["requested_version"].get<std::uint64_t>(), 1U);

    const auto pruned_baseline_diff = facade.get_agent_runtime_diff(agent_a, 0, 4);
    EXPECT_EQ(pruned_baseline_diff.value("error", std::string()), "version_not_found");
    EXPECT_EQ(pruned_baseline_diff["requested_version"].get<std::uint64_t>(), 0U);

    const auto kept_diff = facade.get_agent_runtime_diff(agent_a, 3, 4);
    EXPECT_FALSE(kept_diff.contains("error"));

    const auto unpruned_baseline_diff = facade.get_agent_runtime_diff(agent_b, 0, 2);
    EXPECT_FALSE(unpruned_baseline_diff.contains("error"));
}

TEST_F(IntegrationTest, RuntimeConfigHistoryPrunePersistsAndDoesNotRenumberVersions) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;

    const std::string agent_id = "executor-runtime-prune-persist";

    const auto s0 = make_runtime_snapshot(agent_id, "executor", "persist prompt v0", "1.0.0");
    const auto s1 = make_runtime_snapshot(agent_id, "executor", "persist prompt v1", "1.0.1");
    const auto s2 = make_runtime_snapshot(agent_id, "executor", "persist prompt v2", "1.0.2");
    const auto s3 = make_runtime_snapshot(agent_id, "executor", "persist prompt v3", "1.0.3");
    const auto s4 = make_runtime_snapshot(agent_id, "executor", "persist prompt v4", "1.0.4");
    const auto s5 = make_runtime_snapshot(agent_id, "executor", "persist prompt v5", "1.0.5");
    const auto s6 = make_runtime_snapshot(agent_id, "executor", "persist prompt v6", "1.0.6");

    nlohmann::json history = nlohmann::json::array();
    const auto push_record = [&history](const std::string& id,
                                        const std::uint64_t version,
                                        const nlohmann::json& before,
                                        const nlohmann::json& after,
                                        const std::string& changed_at) {
        history.push_back({
            {"agent_id", id},
            {"version", version},
            {"proposal_id", "proposal-persist-" + std::to_string(version)},
            {"before", before},
            {"after", after},
            {"diff", {{"system_prompt", {{"before", before["system_prompt"]}, {"after", after["system_prompt"]}}}}},
            {"changed_at", changed_at}
        });
    };

    push_record(agent_id, 1U, s0, s1, "2026-03-01T11:00:00");
    push_record(agent_id, 2U, s1, s2, "2026-03-01T11:01:00");
    push_record(agent_id, 3U, s2, s3, "2026-03-01T11:02:00");
    push_record(agent_id, 4U, s3, s4, "2026-03-01T11:03:00");
    push_record(agent_id, 5U, s4, s5, "2026-03-01T11:04:00");
    push_record(agent_id, 6U, s5, s6, "2026-03-01T11:05:00");

    write_json_file(test_dir_ / "runtime_config_versions.json", {
        {agent_id, 6U}
    });
    write_json_file(test_dir_ / "runtime_config_history.json", history);

    {
        evoclaw::facade::EvoClawFacade facade(config);
        facade.initialize();
        facade.register_agent(std::make_shared<evoclaw::agent::Executor>(
            make_agent_config(agent_id, "executor", {"execute"})));

        EXPECT_EQ(facade.get_agent_runtime_version(agent_id)["version"].get<std::uint64_t>(), 6U);
        EXPECT_EQ(facade.get_agent_runtime_history(agent_id).size(), 6U);

        facade.clear_old_runtime_config_history(2);

        const auto pruned_history = facade.get_agent_runtime_history(agent_id);
        ASSERT_EQ(pruned_history.size(), 2U);
        EXPECT_EQ(pruned_history[0]["version"].get<std::uint64_t>(), 5U);
        EXPECT_EQ(pruned_history[1]["version"].get<std::uint64_t>(), 6U);
        EXPECT_EQ(facade.get_agent_runtime_version(agent_id)["version"].get<std::uint64_t>(), 6U);

        facade.save_state();
    }

    {
        evoclaw::facade::EvoClawFacade facade2(config);
        facade2.initialize();
        facade2.register_agent(std::make_shared<evoclaw::agent::Executor>(
            make_agent_config(agent_id, "executor", {"execute"})));

        const auto version_after_load = facade2.get_agent_runtime_version(agent_id);
        EXPECT_EQ(version_after_load["version"].get<std::uint64_t>(), 6U);

        const auto history_after_load = facade2.get_agent_runtime_history(agent_id);
        ASSERT_EQ(history_after_load.size(), 2U);
        EXPECT_EQ(history_after_load[0]["version"].get<std::uint64_t>(), 5U);
        EXPECT_EQ(history_after_load[1]["version"].get<std::uint64_t>(), 6U);

        const auto status = facade2.get_status();
        ASSERT_TRUE(status.contains("runtime_config"));
        EXPECT_EQ(status["runtime_config"]["history_entries"].get<std::size_t>(), 2U);
    }
}

TEST_F(IntegrationTest, RuntimeConfigAutoPruneGovernanceKeepsRecentEntriesAndPersists) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    config.runtime_history_keep_last_per_agent = 2U;

    const std::string agent_id = "executor-runtime-auto-prune";

    const auto s0 = make_runtime_snapshot(agent_id, "executor", "auto prompt v0", "1.0.0");
    const auto s1 = make_runtime_snapshot(agent_id, "executor", "auto prompt v1", "1.0.1");
    const auto s2 = make_runtime_snapshot(agent_id, "executor", "auto prompt v2", "1.0.2");
    const auto s3 = make_runtime_snapshot(agent_id, "executor", "auto prompt v3", "1.0.3");
    const auto s4 = make_runtime_snapshot(agent_id, "executor", "auto prompt v4", "1.0.4");

    nlohmann::json history = nlohmann::json::array();
    const auto push_record = [&history](const std::string& id,
                                        const std::uint64_t version,
                                        const nlohmann::json& before,
                                        const nlohmann::json& after,
                                        const std::string& changed_at) {
        history.push_back({
            {"agent_id", id},
            {"version", version},
            {"proposal_id", "proposal-auto-" + std::to_string(version)},
            {"before", before},
            {"after", after},
            {"diff", {{"system_prompt", {{"before", before["system_prompt"]}, {"after", after["system_prompt"]}}}}},
            {"changed_at", changed_at}
        });
    };

    push_record(agent_id, 1U, s0, s1, "2026-03-01T12:00:00");
    push_record(agent_id, 2U, s1, s2, "2026-03-01T12:01:00");
    push_record(agent_id, 3U, s2, s3, "2026-03-01T12:02:00");
    push_record(agent_id, 4U, s3, s4, "2026-03-01T12:03:00");

    write_json_file(test_dir_ / "runtime_config_versions.json", {
        {agent_id, 4U}
    });
    write_json_file(test_dir_ / "runtime_config_history.json", history);

    {
        evoclaw::facade::EvoClawFacade facade(config);
        facade.initialize();
        facade.register_agent(std::make_shared<evoclaw::agent::Executor>(
            make_agent_config(agent_id, "executor", {"execute"})));

        const auto history_after_init = facade.get_agent_runtime_history(agent_id);
        ASSERT_EQ(history_after_init.size(), 2U);
        EXPECT_EQ(history_after_init[0]["version"].get<std::uint64_t>(), 3U);
        EXPECT_EQ(history_after_init[1]["version"].get<std::uint64_t>(), 4U);

        const auto version_after_init = facade.get_agent_runtime_version(agent_id);
        EXPECT_EQ(version_after_init["version"].get<std::uint64_t>(), 4U);

        const auto status = facade.get_status();
        ASSERT_TRUE(status.contains("runtime_config"));
        ASSERT_TRUE(status["runtime_config"].contains("governance"));
        EXPECT_TRUE(status["runtime_config"]["governance"]["auto_prune_enabled"].get<bool>());
        EXPECT_EQ(status["runtime_config"]["governance"]["keep_last_per_agent"].get<std::size_t>(), 2U);
        EXPECT_EQ(status["runtime_config"]["history_entries"].get<std::size_t>(), 2U);

        const auto old_diff = facade.get_agent_runtime_diff(agent_id, 1U, 2U);
        EXPECT_EQ(old_diff.value("error", std::string()), "version_not_found");

        facade.save_state();
    }

    {
        evoclaw::facade::EvoClawFacade facade2(config);
        facade2.initialize();
        facade2.register_agent(std::make_shared<evoclaw::agent::Executor>(
            make_agent_config(agent_id, "executor", {"execute"})));

        const auto history_after_reload = facade2.get_agent_runtime_history(agent_id);
        ASSERT_EQ(history_after_reload.size(), 2U);
        EXPECT_EQ(history_after_reload[0]["version"].get<std::uint64_t>(), 3U);
        EXPECT_EQ(history_after_reload[1]["version"].get<std::uint64_t>(), 4U);

        const auto version_after_reload = facade2.get_agent_runtime_version(agent_id);
        EXPECT_EQ(version_after_reload["version"].get<std::uint64_t>(), 4U);

        const auto kept_diff = facade2.get_agent_runtime_diff(agent_id, 3U, 4U);
        EXPECT_FALSE(kept_diff.contains("error"));

        const auto pruned_baseline_diff = facade2.get_agent_runtime_diff(agent_id, 0U, 4U);
        EXPECT_EQ(pruned_baseline_diff.value("error", std::string()), "version_not_found");
    }
}


TEST_F(IntegrationTest, RuntimeConfigAutoPruneEmitsAuditablePruneEvent) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    config.runtime_history_keep_last_per_agent = 0U;

    const std::string agent_id = "executor-runtime-auto-prune-event";

    const auto s0 = make_runtime_snapshot(agent_id, "executor", "event prompt v0", "1.0.0");
    const auto s1 = make_runtime_snapshot(agent_id, "executor", "event prompt v1", "1.0.1");
    const auto s2 = make_runtime_snapshot(agent_id, "executor", "event prompt v2", "1.0.2");
    const auto s3 = make_runtime_snapshot(agent_id, "executor", "event prompt v3", "1.0.3");
    const auto s4 = make_runtime_snapshot(agent_id, "executor", "event prompt v4", "1.0.4");

    nlohmann::json history = nlohmann::json::array();
    const auto push_record = [&history](const std::string& id,
                                        const std::uint64_t version,
                                        const nlohmann::json& before,
                                        const nlohmann::json& after,
                                        const std::string& changed_at) {
        history.push_back({
            {"agent_id", id},
            {"version", version},
            {"proposal_id", "proposal-auto-event-" + std::to_string(version)},
            {"before", before},
            {"after", after},
            {"diff", {{"system_prompt", {{"before", before["system_prompt"]}, {"after", after["system_prompt"]}}}}},
            {"changed_at", changed_at}
        });
    };

    push_record(agent_id, 1U, s0, s1, "2026-03-01T13:00:00");
    push_record(agent_id, 2U, s1, s2, "2026-03-01T13:01:00");
    push_record(agent_id, 3U, s2, s3, "2026-03-01T13:02:00");
    push_record(agent_id, 4U, s3, s4, "2026-03-01T13:03:00");

    write_json_file(test_dir_ / "runtime_config_versions.json", {
        {agent_id, 4U}
    });
    write_json_file(test_dir_ / "runtime_config_history.json", history);

    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();
    facade.register_agent(std::make_shared<evoclaw::agent::Executor>(
        make_agent_config(agent_id, "executor", {"execute"})));

    std::vector<nlohmann::json> events;
    facade.set_event_callback([&events](const nlohmann::json& event) {
        events.push_back(event);
    });

    EXPECT_EQ(facade.get_agent_runtime_history(agent_id).size(), 4U);
    EXPECT_EQ(facade.get_agent_runtime_version(agent_id)["version"].get<std::uint64_t>(), 4U);

    facade.set_runtime_history_keep_last_per_agent(2U);

    const auto history_after = facade.get_agent_runtime_history(agent_id);
    ASSERT_EQ(history_after.size(), 2U);
    EXPECT_EQ(history_after[0]["version"].get<std::uint64_t>(), 3U);
    EXPECT_EQ(history_after[1]["version"].get<std::uint64_t>(), 4U);
    EXPECT_EQ(facade.get_agent_runtime_version(agent_id)["version"].get<std::uint64_t>(), 4U);

    const auto prune_event_it = std::find_if(events.begin(), events.end(), [](const nlohmann::json& event) {
        return event.value("event", std::string()) == "runtime_config_history_pruned";
    });
    ASSERT_NE(prune_event_it, events.end());

    const auto& prune_event = *prune_event_it;
    EXPECT_EQ(prune_event.value("trigger", std::string()), "auto");
    EXPECT_EQ(prune_event.value("keep_last_per_agent", 0U), 2U);
    EXPECT_EQ(prune_event.value("history_entries_before", 0U), 4U);
    EXPECT_EQ(prune_event.value("history_entries_after", 0U), 2U);
    EXPECT_EQ(prune_event.value("pruned_entries", 0U), 2U);
    ASSERT_TRUE(prune_event.contains("removed_by_agent"));
    ASSERT_TRUE(prune_event["removed_by_agent"].is_array());
    ASSERT_EQ(prune_event["removed_by_agent"].size(), 1U);
    EXPECT_EQ(prune_event["removed_by_agent"][0]["agent_id"].get<std::string>(), agent_id);
    EXPECT_EQ(prune_event["removed_by_agent"][0]["removed_entries"].get<std::size_t>(), 2U);
}

TEST_F(IntegrationTest, GovernanceConfigPersistsAcrossRestart) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    config.runtime_history_keep_last_per_agent = 0U;

    {
        evoclaw::facade::EvoClawFacade facade(config);
        facade.initialize();
        facade.register_agent(std::make_shared<evoclaw::agent::Executor>(
            make_agent_config("executor-gov-persist", "executor", {"execute"})));

        EXPECT_EQ(facade.get_runtime_governance_status()["keep_last_per_agent"].get<std::size_t>(), 0U);

        facade.set_runtime_history_keep_last_per_agent(3U);

        EXPECT_EQ(facade.get_runtime_governance_status()["keep_last_per_agent"].get<std::size_t>(), 3U);
    }

    EXPECT_TRUE(std::filesystem::exists(test_dir_ / "governance_config.json"));

    {
        evoclaw::facade::EvoClawFacade facade2(config);
        facade2.initialize();
        facade2.register_agent(std::make_shared<evoclaw::agent::Executor>(
            make_agent_config("executor-gov-persist", "executor", {"execute"})));

        EXPECT_EQ(facade2.get_runtime_governance_status()["keep_last_per_agent"].get<std::size_t>(), 3U);
    }
}

TEST_F(IntegrationTest, GovernanceConfigUsesDefaultWhenFileMissing) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;

    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();
    facade.register_agent(std::make_shared<evoclaw::agent::Executor>(
        make_agent_config("executor-gov-default", "executor", {"execute"})));

    EXPECT_EQ(facade.get_runtime_governance_status()["keep_last_per_agent"].get<std::size_t>(), 0U);
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


TEST_F(IntegrationTest, SnapshotPersistenceRoundTrip) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    config.evolver_config.kpi_decline_threshold = 0.95;
    config.evolver_config.min_sample_size = 8;
    config.evolver_config.confidence_threshold = 0.7;
    config.evolver_config.max_proposals_per_cycle = 3;
    config.evolver_config.consecutive_failures = 100;

    {
        evoclaw::facade::EvoClawFacade facade(config);
        facade.initialize();

        auto executor = std::make_shared<evoclaw::agent::Executor>(
            make_agent_config("executor-persist", "executor", {"execute"}));
        facade.register_agent(executor);

        for (int i = 0; i < 10; ++i) {
            evoclaw::agent::Task task;
            task.id = "persist-kpi-" + std::to_string(i);
            task.description = "score task";
            task.type = evoclaw::TaskType::ROUTE;
            task.context["intent"] = "execute";
            task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
            task.context["score"] = (i < 6) ? 0.9 : 0.8;
            (void)facade.submit_task(task);
        }

        facade.trigger_evolution();

        const auto snapshots_before = facade.list_rollback_snapshots();
        ASSERT_FALSE(snapshots_before.empty());

        facade.save_state();
    }

    EXPECT_TRUE(std::filesystem::exists(test_dir_ / "snapshots.json"));

    {
        evoclaw::facade::EvoClawFacade facade2(config);
        facade2.initialize();

        auto executor2 = std::make_shared<evoclaw::agent::Executor>(
            make_agent_config("executor-persist", "executor", {"execute"}));
        facade2.register_agent(executor2);

        const auto snapshots_after = facade2.list_rollback_snapshots();
        ASSERT_FALSE(snapshots_after.empty());
        EXPECT_EQ(snapshots_after[0]["agent_id"].get<std::string>(), "executor-persist");

        const auto pid = snapshots_after[0]["proposal_id"].get<std::string>();
        std::string reason;
        const bool rolled_back = facade2.rollback_proposal(pid, &reason);
        EXPECT_TRUE(rolled_back) << reason;
    }
}

TEST_F(IntegrationTest, ClearExpiredSnapshotsRemovesOldEntries) {
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
        make_agent_config("executor-expire", "executor", {"execute"}));
    facade.register_agent(executor);

    for (int i = 0; i < 10; ++i) {
        evoclaw::agent::Task task;
        task.id = "expire-kpi-" + std::to_string(i);
        task.description = "score task";
        task.type = evoclaw::TaskType::ROUTE;
        task.context["intent"] = "execute";
        task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
        task.context["score"] = (i < 6) ? 0.9 : 0.8;
        (void)facade.submit_task(task);
    }

    facade.trigger_evolution();

    const auto before = facade.list_rollback_snapshots();
    ASSERT_FALSE(before.empty());

    facade.clear_expired_snapshots(std::chrono::seconds(0));

    const auto after = facade.list_rollback_snapshots();
    EXPECT_TRUE(after.empty());
}


TEST_F(IntegrationTest, StatusIncludesRollbackSnapshotsAndHistory) {
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
        make_agent_config("executor-status", "executor", {"execute"}));
    facade.register_agent(executor);

    for (int i = 0; i < 10; ++i) {
        evoclaw::agent::Task task;
        task.id = "status-kpi-" + std::to_string(i);
        task.description = "score task";
        task.type = evoclaw::TaskType::ROUTE;
        task.context["intent"] = "execute";
        task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
        task.context["score"] = (i < 6) ? 0.9 : 0.8;
        (void)facade.submit_task(task);
    }

    facade.trigger_evolution();

    const auto status = facade.get_status();
    ASSERT_TRUE(status.contains("rollback_snapshots"));
    ASSERT_TRUE(status["rollback_snapshots"].is_array());
    EXPECT_FALSE(status["rollback_snapshots"].empty());

    const auto history = facade.get_evolution_history();
    ASSERT_TRUE(history.is_array());
    EXPECT_FALSE(history.empty());
    EXPECT_TRUE(history.back().contains("proposals"));
}

TEST_F(IntegrationTest, AgentEvolutionStatsSummarizeHistory) {
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
        make_agent_config("executor-stats", "executor", {"execute"}));
    facade.register_agent(executor);

    for (int i = 0; i < 10; ++i) {
        evoclaw::agent::Task task;
        task.id = "stats-kpi-" + std::to_string(i);
        task.description = "score task";
        task.type = evoclaw::TaskType::ROUTE;
        task.context["intent"] = "execute";
        task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
        task.context["score"] = (i < 6) ? 0.9 : 0.8;
        (void)facade.submit_task(task);
    }

    facade.trigger_evolution();

    const auto stats = facade.get_agent_evolution_stats("executor-stats");
    EXPECT_EQ(stats["agent_id"].get<std::string>(), "executor-stats");
    EXPECT_GE(stats["total_proposals"].get<int>(), 1);
    EXPECT_GE(stats["applied_proposals"].get<int>(), 1);
    EXPECT_GE(stats["best_improvement"].get<double>(), 0.0);
    EXPECT_GE(stats["average_improvement"].get<double>(), 0.0);
}


TEST_F(IntegrationTest, EvolutionHistoryPersistenceRoundTrip) {
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = test_dir_;
    config.evolver_config.kpi_decline_threshold = 0.95;
    config.evolver_config.min_sample_size = 8;
    config.evolver_config.confidence_threshold = 0.7;
    config.evolver_config.max_proposals_per_cycle = 3;
    config.evolver_config.consecutive_failures = 100;

    {
        evoclaw::facade::EvoClawFacade facade(config);
        facade.initialize();

        auto executor = std::make_shared<evoclaw::agent::Executor>(
            make_agent_config("executor-history", "executor", {"execute"}));
        facade.register_agent(executor);

        for (int i = 0; i < 10; ++i) {
            evoclaw::agent::Task task;
            task.id = "history-kpi-" + std::to_string(i);
            task.description = "score task";
            task.type = evoclaw::TaskType::ROUTE;
            task.context["intent"] = "execute";
            task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
            task.context["score"] = (i < 6) ? 0.9 : 0.8;
            (void)facade.submit_task(task);
        }

        facade.trigger_evolution();
        const auto history_before = facade.get_evolution_history();
        ASSERT_FALSE(history_before.empty());
        facade.save_state();
    }

    EXPECT_TRUE(std::filesystem::exists(test_dir_ / "evolution_history.json"));

    {
        evoclaw::facade::EvoClawFacade facade2(config);
        facade2.initialize();
        const auto history_after = facade2.get_evolution_history();
        ASSERT_FALSE(history_after.empty());
        EXPECT_TRUE(history_after.back().contains("proposals"));

        const auto status = facade2.get_status();
        ASSERT_TRUE(status.contains("evolution"));
        EXPECT_EQ(status["evolution"]["last_cycle"], history_after.back());
    }
}

TEST_F(IntegrationTest, ClearOldEvolutionHistoryKeepsRecentEntries) {
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
        make_agent_config("executor-history-prune", "executor", {"execute"}));
    facade.register_agent(executor);

    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 10; ++i) {
            evoclaw::agent::Task task;
            task.id = "history-prune-" + std::to_string(cycle) + "-" + std::to_string(i);
            task.description = "score task";
            task.type = evoclaw::TaskType::ROUTE;
            task.context["intent"] = "execute";
            task.context["critic_score"] = (i < 6) ? 0.9 : 0.8;
            task.context["score"] = (i < 6) ? 0.9 : 0.8;
            (void)facade.submit_task(task);
        }
        facade.trigger_evolution();
    }

    const auto before = facade.get_evolution_history();
    ASSERT_GE(before.size(), 3U);

    facade.clear_old_evolution_history(2);
    const auto after = facade.get_evolution_history();
    EXPECT_EQ(after.size(), 2U);
    EXPECT_EQ(after[0], before[before.size() - 2]);
    EXPECT_EQ(after[1], before[before.size() - 1]);
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
