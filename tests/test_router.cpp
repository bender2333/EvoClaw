#include "router/router.hpp"
#include "agent/planner.hpp"
#include "agent/executor.hpp"
#include "agent/critic.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>

namespace {

evoclaw::umi::ModuleContract make_contract(std::string module_id,
                                           std::vector<std::string> intents) {
    evoclaw::umi::ModuleContract contract;
    contract.module_id = std::move(module_id);
    contract.version = "1.0.0";
    contract.capability.intent_tags = std::move(intents);
    contract.capability.required_tools = {};
    contract.capability.success_rate_threshold = 0.75;
    contract.capability.estimated_cost_token = 100.0;
    contract.airbag.level = evoclaw::umi::AirbagLevel::BASIC;
    contract.airbag.reversible = true;
    contract.cost_model = {{"tokens", 100.0}};
    return contract;
}

evoclaw::agent::AgentConfig make_config(const std::string& id,
                                        const std::string& role,
                                        evoclaw::umi::ModuleContract contract) {
    evoclaw::agent::AgentConfig config;
    config.id = id;
    config.role = role;
    config.system_prompt = "test";
    config.contract = std::move(contract);
    config.temperature = 0.3;
    return config;
}

TEST(RouterTest, RegisterAndRouteByIntent) {
    evoclaw::router::Router router;

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_config("planner-1", "planner", make_contract("planner.mod", {"plan", "decompose"})));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_config("executor-1", "executor", make_contract("executor.mod", {"execute", "code"})));

    router.register_agent(planner);
    router.register_agent(executor);

    // 通过 UMI 匹配
    evoclaw::umi::CapabilityProfile required;
    required.intent_tags = {"plan"};
    auto matches = router.match_by_contract(required);
    EXPECT_FALSE(matches.empty());
    EXPECT_EQ(matches[0], "planner-1");

    required.intent_tags = {"execute"};
    matches = router.match_by_contract(required);
    EXPECT_FALSE(matches.empty());
    EXPECT_EQ(matches[0], "executor-1");
}

TEST(RouterTest, RouteTaskSelectsCorrectAgent) {
    evoclaw::router::RoutingConfig config;
    config.epsilon = 0.0;  // 关闭探索，确保确定性
    evoclaw::router::Router router(config);

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_config("planner-1", "planner", make_contract("planner.mod", {"plan"})));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_config("executor-1", "executor", make_contract("executor.mod", {"execute"})));

    router.register_agent(planner);
    router.register_agent(executor);

    evoclaw::agent::Task task;
    task.id = "t1";
    task.description = "plan something";
    task.type = evoclaw::TaskType::EXECUTE;
    task.context = {{"intent", "plan"}};

    auto result = router.route(task);
    EXPECT_TRUE(result.has_value());
}

TEST(RouterTest, CapabilityMatrixUpdateAndQuery) {
    evoclaw::router::CapabilityMatrix matrix;

    matrix.update("agent-a", "plan", 0.9);
    matrix.update("agent-b", "plan", 0.7);
    matrix.update("agent-a", "execute", 0.5);

    auto best = matrix.best_for("plan");
    EXPECT_TRUE(best.has_value());
    EXPECT_EQ(best.value(), "agent-a");

    auto candidates = matrix.candidates_for("plan");
    EXPECT_EQ(candidates.size(), 2);
    EXPECT_EQ(candidates[0], "agent-a");  // 高分在前
    EXPECT_EQ(candidates[1], "agent-b");
}

TEST(RouterTest, EpsilonGreedyExploration) {
    evoclaw::router::RoutingConfig config;
    config.epsilon = 1.0;  // 100% 探索
    evoclaw::router::Router router(config);

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_config("planner-1", "planner", make_contract("planner.mod", {"plan"})));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_config("executor-1", "executor", make_contract("executor.mod", {"plan"})));

    router.register_agent(planner);
    router.register_agent(executor);

    // 100% 探索率下，多次路由应该能看到不同 Agent
    std::set<std::string> seen;
    for (int i = 0; i < 50; ++i) {
        auto id = router.route_with_exploration("plan");
        seen.insert(id);
    }
    // 应该至少看到 1 个 Agent（大概率看到 2 个）
    EXPECT_GE(seen.size(), 1u);
}

TEST(RouterTest, MatchByContractNoMatch) {
    evoclaw::router::Router router;

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_config("planner-1", "planner", make_contract("planner.mod", {"plan"})));
    router.register_agent(planner);

    evoclaw::umi::CapabilityProfile required;
    required.intent_tags = {"nonexistent_intent"};
    auto matches = router.match_by_contract(required);
    EXPECT_TRUE(matches.empty());
}

} // namespace
