#include "agent/critic.hpp"
#include "agent/executor.hpp"
#include "agent/planner.hpp"
#include "facade/facade.hpp"
#include "server/server.hpp"

#include <iostream>
#include <memory>
#include <vector>

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
    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = "./server_demo_logs";

    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();

    const auto status = facade.get_status();
    if (status.contains("llm") && status["llm"].is_object()) {
        const auto& llm = status["llm"];
        std::cout << "LLM model: " << llm.value("model", std::string("unknown")) << '\n';
        std::cout << "LLM base URL: " << llm.value("base_url", std::string("unknown")) << '\n';
        std::cout << "LLM mock mode: " << (llm.value("mock_mode", true) ? "enabled" : "disabled") << '\n';
    }

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_config("planner-1", "planner", {"plan", "decompose"}));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_config("executor-1", "executor", {"execute", "code"}));
    auto critic = std::make_shared<evoclaw::agent::Critic>(
        make_config("critic-1", "critic", {"critique", "evaluate"}));

    facade.register_agent(planner);
    facade.register_agent(executor);
    facade.register_agent(critic);

    evoclaw::server::EvoClawServer server(facade, 8080);

    std::cout << "Dashboard: http://localhost:8080" << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    server.start();
    return 0;
}
