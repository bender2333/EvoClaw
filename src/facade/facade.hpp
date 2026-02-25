#pragma once

#include "agent/agent.hpp"
#include "evolution/evolver.hpp"
#include "event_log/event_log.hpp"
#include "governance/governance_kernel.hpp"
#include "memory/org_log.hpp"
#include "memory/working_memory.hpp"
#include "protocol/bus.hpp"
#include "router/router.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace evoclaw::facade {

class EvoClawFacade {
public:
    struct Config {
        std::filesystem::path log_dir = "./logs";
        std::filesystem::path config_path = "./constitution.json";
        router::RoutingConfig router_config;
        evolution::Evolver::Config evolver_config;
    };

    explicit EvoClawFacade(Config config);
    EvoClawFacade();

    void initialize();
    void register_agent(std::shared_ptr<agent::Agent> agent);
    [[nodiscard]] agent::TaskResult submit_task(const agent::Task& task);
    void trigger_evolution();
    [[nodiscard]] nlohmann::json get_status() const;
    [[nodiscard]] nlohmann::json get_capability_matrix() const;
    [[nodiscard]] bool verify_event_log() const;

private:
    Config config_;
    bool initialized_ = false;

    std::shared_ptr<protocol::MessageBus> bus_;
    std::unique_ptr<router::Router> router_;
    std::unique_ptr<memory::WorkingMemory> working_memory_;
    std::unique_ptr<memory::OrgLog> org_log_;
    std::unique_ptr<event_log::EventLog> event_log_;
    std::unique_ptr<governance::GovernanceKernel> governance_;
    std::unique_ptr<evolution::Evolver> evolver_;

    std::unordered_map<AgentId, std::shared_ptr<agent::Agent>> agents_;
    nlohmann::json last_evolution_report_;

    static governance::Constitution load_constitution(const std::filesystem::path& path);
    void run_evolution_cycle();
    void ensure_initialized() const;
};

} // namespace evoclaw::facade
