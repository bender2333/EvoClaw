#include "facade/facade.hpp"
#include "agent/planner.hpp"
#include "agent/executor.hpp"
#include "agent/critic.hpp"

#include <gtest/gtest.h>
#include <filesystem>

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

} // namespace
