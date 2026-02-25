#include "agent/critic.hpp"
#include "agent/executor.hpp"
#include "agent/planner.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

evoclaw::umi::ModuleContract make_contract(std::string module_id,
                                           std::vector<std::string> intents,
                                           std::vector<std::string> tools,
                                           double success_rate_threshold = 0.75,
                                           double estimated_cost_token = 128.0) {
    evoclaw::umi::ModuleContract contract;
    contract.module_id = std::move(module_id);
    contract.version = "1.0.0";
    contract.capability.intent_tags = std::move(intents);
    contract.capability.required_tools = std::move(tools);
    contract.capability.success_rate_threshold = success_rate_threshold;
    contract.capability.estimated_cost_token = estimated_cost_token;
    contract.airbag.level = evoclaw::umi::AirbagLevel::BASIC;
    contract.airbag.reversible = true;
    contract.cost_model = {{"tokens", estimated_cost_token}};
    return contract;
}

evoclaw::agent::AgentConfig make_config(const std::string& id,
                                        const std::string& role,
                                        evoclaw::umi::ModuleContract contract) {
    evoclaw::agent::AgentConfig config;
    config.id = id;
    config.role = role;
    config.system_prompt = "test-system";
    config.contract = std::move(contract);
    config.temperature = 0.2;
    return config;
}

TEST(AgentTest, PlannerExecuteAndReflect) {
    evoclaw::agent::Planner planner(make_config(
        "planner-1", "planner", make_contract("planner.module", {"plan"}, {"llm", "memory"})));

    evoclaw::agent::Task task;
    task.id = "task-plan";
    task.description = "Draft implementation plan";
    task.context = {{"constraints", {"keep tests deterministic"}}};
    task.type = evoclaw::TaskType::EXECUTE;

    const auto result = planner.execute(task);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.agent_id, "planner-1");
    EXPECT_NE(result.output.find("Generated plan"), std::string::npos);
    EXPECT_TRUE(result.metadata.contains("steps"));
    EXPECT_GT(result.confidence, 0.0);

    const auto reflection = planner.reflect(result);
    EXPECT_EQ(reflection.agent_id, "planner-1");
    EXPECT_FALSE(reflection.summary.empty());
    EXPECT_FALSE(reflection.improvements.empty());
}

TEST(AgentTest, ExecutorExecuteAndReflect) {
    evoclaw::agent::Executor executor(make_config(
        "executor-1", "executor", make_contract("executor.module", {"execute"}, {"shell"})));

    evoclaw::agent::Task task;
    task.id = "task-exec";
    task.description = "Fallback plan";
    task.context = {{"plan", "run command pipeline"}};
    task.type = evoclaw::TaskType::EXECUTE;

    const auto result = executor.execute(task);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.agent_id, "executor-1");
    EXPECT_NE(result.output.find("Executed plan: run command pipeline"), std::string::npos);
    EXPECT_TRUE(result.metadata.contains("plan_used"));
    EXPECT_GT(result.confidence, 0.0);

    const auto reflection = executor.reflect(result);
    EXPECT_EQ(reflection.agent_id, "executor-1");
    EXPECT_FALSE(reflection.summary.empty());
    EXPECT_FALSE(reflection.improvements.empty());
}

TEST(AgentTest, CriticExecuteAndReflect) {
    evoclaw::agent::Critic critic(make_config(
        "critic-1", "critic", make_contract("critic.module", {"critique"}, {"analysis"})));

    evoclaw::agent::Task task;
    task.id = "task-crit";
    task.description = "Evaluate execution output";
    task.context = {
        {"target_agent", "executor-1"},
        {"success", true},
        {"result_output", "execution output"},
        {"result_confidence", 0.8}
    };
    task.type = evoclaw::TaskType::CRITIQUE;

    const auto result = critic.execute(task);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.agent_id, "critic-1");
    EXPECT_TRUE(result.metadata.contains("score"));
    EXPECT_TRUE(result.metadata.contains("verdict"));
    EXPECT_NE(result.output.find("Critic verdict"), std::string::npos);

    const double score = result.metadata.at("score").get<double>();
    EXPECT_GE(score, 0.0);
    EXPECT_LE(score, 1.0);

    const auto reflection = critic.reflect(result);
    EXPECT_EQ(reflection.agent_id, "critic-1");
    EXPECT_FALSE(reflection.summary.empty());
    EXPECT_FALSE(reflection.improvements.empty());
}

} // namespace
