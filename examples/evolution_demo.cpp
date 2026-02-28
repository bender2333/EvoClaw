#include "agent/critic.hpp"
#include "agent/executor.hpp"
#include "agent/planner.hpp"
#include "facade/facade.hpp"

#include <filesystem>
#include <iostream>
#include <string>
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

void print_evolution_chain(const nlohmann::json& status) {
    if (!status.contains("evolution") || !status["evolution"].is_object()) {
        std::cout << "No evolution report found." << std::endl;
        return;
    }

    const auto last_cycle = status["evolution"].value("last_cycle", nlohmann::json::object());
    if (!last_cycle.is_object()) {
        std::cout << "Invalid evolution report format." << std::endl;
        return;
    }

    std::cout << "\n=== Evolution Chain ===" << std::endl;
    std::cout << "tensions=" << last_cycle.value("tension_count", 0)
              << ", proposals=" << last_cycle.value("proposal_count", 0)
              << ", applied=" << last_cycle.value("applied_count", 0) << std::endl;

    const auto tensions = last_cycle.value("tensions", nlohmann::json::array());
    for (const auto& tension : tensions) {
        std::cout << "TENSION " << tension.value("id", "")
                  << " [" << tension.value("type", "") << "]"
                  << " agent=" << tension.value("source_agent", "")
                  << " severity=" << tension.value("severity", "") << std::endl;
    }

    const auto proposals = last_cycle.value("proposals", nlohmann::json::array());
    for (const auto& proposal : proposals) {
        const auto ab = proposal.value("ab_test", nlohmann::json::object());
        std::cout << "PROPOSAL " << proposal.value("id", "")
                  << " -> " << (proposal.value("applied", false) ? "APPLY" : "REJECT")
                  << " | improvement=" << ab.value("improvement", 0.0)
                  << " | sample=" << ab.value("sample_size", 0)
                  << " | confidence=" << ab.value("confidence", 0.0)
                  << " | reason=" << proposal.value("rejection_reason", "none");
        if (proposal.value("runtime_patch_applied", false)) {
            std::cout << " | PATCHED";
        }
        std::cout << std::endl;
    }
}

} // namespace

int main() {
    std::cout << "=== EvoClaw Evolution Demo (P4+P5) ===" << std::endl;

    evoclaw::facade::EvoClawFacade::Config config;
    config.log_dir = "./demo_logs";
    config.evolver_config.kpi_decline_threshold = 0.9;
    config.evolver_config.consecutive_failures = 3;
    config.evolver_config.max_proposals_per_cycle = 5;
    evoclaw::facade::EvoClawFacade facade(config);
    facade.initialize();

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

    std::cout << "\nRunning tasks to create mixed performance signals..." << std::endl;

    for (int i = 0; i < 6; ++i) {
        evoclaw::agent::Task task;
        task.id = "demo-ok-" + std::to_string(i);
        task.description = "Write one concise sentence about code quality.";
        task.type = evoclaw::TaskType::EXECUTE;
        task.budget.max_tokens = 512;
        task.budget.max_rounds = 3;
        task.budget.max_tool_calls = 3;

        auto result = facade.submit_task(task);
        std::cout << "  " << task.id << " -> " << (result.success ? "SUCCESS" : "FAILED") << std::endl;
    }

    for (int i = 0; i < 4; ++i) {
        evoclaw::agent::Task task;
        task.id = "demo-fail-" + std::to_string(i);
        task.description = "Route-only degraded execution sample.";
        task.type = evoclaw::TaskType::ROUTE;
        task.context["intent"] = "execute";
        task.context["critic_score"] = 0.1;
        task.context["score"] = 0.1;
        task.budget.max_tokens = 256;

        auto result = facade.submit_task(task);
        std::cout << "  " << task.id << " -> " << (result.success ? "SUCCESS" : "FAILED")
                  << " (forced_score=0.1)" << std::endl;
    }

    std::cout << "\nTriggering evolution..." << std::endl;
    facade.trigger_evolution();

    const auto status = facade.get_status();
    print_evolution_chain(status);

    // --- P5: Runtime Config & Rollback Demo ---
    std::cout << "\n=== P5: Runtime Config & Rollback ===" << std::endl;

    const auto before_config = facade.get_agent_runtime_config("executor-1");
    std::cout << "executor-1 temperature (before): "
              << before_config.value("temperature", -1.0) << std::endl;

    const auto snapshots = facade.list_rollback_snapshots();
    std::cout << "Rollback snapshots stored: " << snapshots.size() << std::endl;

    for (const auto& snap : snapshots) {
        const auto pid = snap.value("proposal_id", "");
        std::cout << "  snapshot: proposal=" << pid
                  << " agent=" << snap.value("agent_id", "") << std::endl;

        std::string reason;
        const bool rolled_back = facade.rollback_proposal(pid, &reason);
        std::cout << "  rollback " << pid << ": "
                  << (rolled_back ? "OK" : ("FAIL: " + reason)) << std::endl;
    }

    const auto after_config = facade.get_agent_runtime_config("executor-1");
    std::cout << "executor-1 temperature (after rollback): "
              << after_config.value("temperature", -1.0) << std::endl;

    // Schema validation demo
    std::string schema_err;
    const bool valid = evoclaw::facade::EvoClawFacade::validate_patch_schema(
        {{"temperature", 0.5}, {"system_prompt_suffix", "be careful"}}, &schema_err);
    std::cout << "\nSchema validation (valid patch): " << (valid ? "PASS" : "FAIL") << std::endl;

    const bool invalid = evoclaw::facade::EvoClawFacade::validate_patch_schema(
        {{"temperature", 0.5}, {"evil_field", "drop table"}}, &schema_err);
    std::cout << "Schema validation (invalid patch): " << (invalid ? "PASS" : ("REJECT: " + schema_err)) << std::endl;

    const bool integrity = facade.verify_event_log();
    std::cout << "\nEvent log integrity: " << (integrity ? "PASS" : "FAIL") << std::endl;

    std::error_code ec;
    std::filesystem::remove_all("./demo_logs", ec);

    return integrity ? 0 : 1;
}
