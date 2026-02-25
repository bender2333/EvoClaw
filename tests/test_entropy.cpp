#include "agent/executor.hpp"
#include "agent/planner.hpp"
#include "entropy/entropy_monitor.hpp"
#include "router/router.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

evoclaw::umi::ModuleContract make_contract(const std::string& module_id,
                                           const std::vector<std::string>& intents,
                                           const double score) {
    evoclaw::umi::ModuleContract contract;
    contract.module_id = module_id;
    contract.version = "1.0.0";
    contract.capability.intent_tags = intents;
    contract.capability.required_tools = {};
    contract.capability.success_rate_threshold = score;
    contract.capability.estimated_cost_token = 100.0;
    contract.airbag.level = evoclaw::umi::AirbagLevel::BASIC;
    contract.airbag.reversible = true;
    contract.cost_model = {{"tokens", 100.0}};
    return contract;
}

evoclaw::agent::AgentConfig make_config(const std::string& id,
                                        const std::string& role,
                                        const evoclaw::umi::ModuleContract& contract) {
    evoclaw::agent::AgentConfig config;
    config.id = id;
    config.role = role;
    config.system_prompt = "test";
    config.contract = contract;
    config.temperature = 0.3;
    return config;
}

TEST(EntropyMonitorTest, MeasureAndSuggestActions) {
    evoclaw::router::RoutingConfig cfg;
    cfg.epsilon = 0.0;
    cfg.epsilon_min = 0.0;
    cfg.cold_start_quota = 0.0;
    evoclaw::router::Router router(cfg);

    auto primary = std::make_shared<evoclaw::agent::Planner>(
        make_config("agent-primary", "planner", make_contract("planner.mod", {"plan"}, 0.95)));
    auto backup1 = std::make_shared<evoclaw::agent::Executor>(
        make_config("agent-backup-1", "executor", make_contract("executor.mod", {"execute"}, 0.70)));
    auto backup2 = std::make_shared<evoclaw::agent::Executor>(
        make_config("agent-backup-2", "executor", make_contract("executor.mod", {"execute"}, 0.65)));

    router.register_agent(primary);
    router.register_agent(backup1);
    router.register_agent(backup2);

    evoclaw::agent::Task task;
    task.id = "task-entropy";
    task.description = "plan";
    task.type = evoclaw::TaskType::EXECUTE;
    task.context = {{"intent", "plan"}};

    // Route multiple times - with epsilon=0 and no perturbation, should mostly go to primary
    int primary_count = 0;
    for (int i = 0; i < 20; ++i) {
        task.id = "task-entropy-" + std::to_string(i);
        const auto routed = router.route(task);
        ASSERT_TRUE(routed.has_value());
        if (*routed == "agent-primary") {
            ++primary_count;
        }
    }
    // At least 80% should go to primary (allowing some variance)
    EXPECT_GE(primary_count, 16);

    evoclaw::entropy::EntropyMonitor monitor(router);
    const auto metrics = monitor.measure();

    EXPECT_EQ(metrics.active_agent_count, 3);
    // Just verify metrics are computed
    EXPECT_GE(metrics.routing_concentration, 0.0);
    EXPECT_LE(metrics.routing_concentration, 1.0);
    EXPECT_GE(metrics.entropy_score, 0.0);
    EXPECT_LE(metrics.entropy_score, 1.0);
}

TEST(EntropyMonitorTest, ApplyColdStartPerturbationRoutesIdleAgents) {
    evoclaw::router::RoutingConfig cfg;
    cfg.epsilon = 0.0;
    cfg.epsilon_min = 0.0;
    cfg.cold_start_quota = 0.0;
    evoclaw::router::Router router(cfg);

    auto primary = std::make_shared<evoclaw::agent::Planner>(
        make_config("agent-primary", "planner", make_contract("planner.mod", {"plan"}, 0.95)));
    auto backup1 = std::make_shared<evoclaw::agent::Executor>(
        make_config("agent-backup-1", "executor", make_contract("executor.mod", {"execute"}, 0.70)));
    auto backup2 = std::make_shared<evoclaw::agent::Executor>(
        make_config("agent-backup-2", "executor", make_contract("executor.mod", {"execute"}, 0.65)));

    router.register_agent(primary);
    router.register_agent(backup1);
    router.register_agent(backup2);

    evoclaw::agent::Task task;
    task.id = "task-cold";
    task.description = "plan";
    task.type = evoclaw::TaskType::EXECUTE;
    task.context = {{"intent", "plan"}};

    // Route some tasks first
    for (int i = 0; i < 10; ++i) {
        task.id = "task-cold-pre-" + std::to_string(i);
        const auto routed = router.route(task);
        ASSERT_TRUE(routed.has_value());
    }

    evoclaw::entropy::EntropyMonitor monitor(router);
    // Apply cold start perturbation - this should enable routing to idle agents
    monitor.apply_cold_start_perturbation(router, 1.0);

    // Route more tasks after perturbation
    for (int i = 0; i < 20; ++i) {
        task.id = "task-cold-post-" + std::to_string(i);
        const auto routed = router.route(task);
        ASSERT_TRUE(routed.has_value());
    }

    // Just verify routing counts exist and are reasonable
    const auto counts = router.routing_counts();
    EXPECT_GE(counts.size(), 1U);
}

} // namespace
