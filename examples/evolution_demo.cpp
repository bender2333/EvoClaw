#include "facade/facade.hpp"
#include "agent/planner.hpp"
#include "agent/executor.hpp"
#include "agent/critic.hpp"

#include <iostream>

namespace {

evoclaw::agent::AgentConfig make_config(const std::string& id,
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

} // namespace

int main() {
    std::cout << "=== EvoClaw Evolution Demo ===" << std::endl;

    // 初始化
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = "./demo_logs";
    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();
    std::cout << "Facade initialized" << std::endl;

    // 注册 Agents
    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_config("planner-1", "planner", {"plan", "decompose"}));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_config("executor-1", "executor", {"execute", "code"}));
    auto critic = std::make_shared<evoclaw::agent::Critic>(
        make_config("critic-1", "critic", {"critique", "evaluate"}));

    facade.register_agent(planner);
    facade.register_agent(executor);
    facade.register_agent(critic);
    std::cout << "Agents registered: planner, executor, critic" << std::endl;

    // 运行任务
    std::cout << "\nRunning 10 tasks..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        evoclaw::agent::Task task;
        task.id = "demo-task-" + std::to_string(i);
        task.description = (i % 2 == 0) ? "Plan and execute strategy" : "Execute and review";
        task.type = evoclaw::TaskType::EXECUTE;

        auto result = facade.submit_task(task);
        std::cout << "  Task " << task.id << ": "
                  << (result.success ? "SUCCESS" : "FAILED")
                  << " [" << result.agent_id << "]" << std::endl;
    }

    // 系统状态
    std::cout << "\n=== System Status ===" << std::endl;
    auto status = facade.get_status();
    std::cout << status.dump(2) << std::endl;

    // 触发进化
    std::cout << "\n=== Triggering Evolution ===" << std::endl;
    facade.trigger_evolution();
    std::cout << "Evolution cycle completed" << std::endl;

    // 验证事件日志
    std::cout << "\n=== Event Log Integrity ===" << std::endl;
    bool integrity = facade.verify_event_log();
    std::cout << "Integrity check: " << (integrity ? "PASS" : "FAIL") << std::endl;

    std::cout << "\n=== Demo Complete ===" << std::endl;

    // 清理
    std::error_code ec;
    std::filesystem::remove_all("./demo_logs", ec);

    return integrity ? 0 : 1;
}
