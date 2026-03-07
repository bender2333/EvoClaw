#pragma once

#include "agent/agent.hpp"
#include "budget/budget_tracker.hpp"
#include "compiler/compiler.hpp"
#include "entropy/entropy_monitor.hpp"
#include "evolution/evolver.hpp"
#include "event_log/event_log.hpp"
#include "governance/governance_kernel.hpp"
#include "llm/llm_client.hpp"
#include "memory/org_log.hpp"
#include "memory/working_memory.hpp"
#include "protocol/bus.hpp"
#include "router/router.hpp"
#include "slp/semantic_primitive.hpp"
#include "zone/zone_manager.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace evoclaw::facade {

class EvoClawFacade {
public:
    using EventCallback = std::function<void(const nlohmann::json&)>;

    struct Config {
        std::filesystem::path log_dir = "./logs";
        std::filesystem::path config_path = "./constitution.json";
        router::RoutingConfig router_config;
        evolution::Evolver::Config evolver_config;
        int global_token_limit = 0;
    };

    struct RuntimeConfigVersionRecord {
        AgentId agent_id;
        std::uint64_t version = 0;
        std::string proposal_id;
        nlohmann::json before;
        nlohmann::json after;
        nlohmann::json diff;
        Timestamp changed_at;
    };

    explicit EvoClawFacade(Config config);
    EvoClawFacade();

    void initialize();
    void set_event_callback(EventCallback callback);
    void register_agent(std::shared_ptr<agent::Agent> agent);
    void register_agent(std::shared_ptr<agent::Agent> agent, zone::Zone zone);
    [[nodiscard]] agent::TaskResult submit_task(const agent::Task& task);
    void save_state() const;
    void trigger_evolution();
    [[nodiscard]] std::vector<memory::OrgLogEntry> query_logs(Timestamp start, Timestamp end) const;
    [[nodiscard]] memory::OrgLog::TimeRangeStats get_log_stats(Timestamp start, Timestamp end) const;
    [[nodiscard]] nlohmann::json get_status() const;
    [[nodiscard]] nlohmann::json get_budget_report() const;
    [[nodiscard]] nlohmann::json get_evolution_budget_status() const;
    [[nodiscard]] nlohmann::json get_capability_matrix() const;
    [[nodiscard]] nlohmann::json get_zone_status() const;
    [[nodiscard]] nlohmann::json get_pattern_status() const;
    [[nodiscard]] nlohmann::json get_entropy_status() const;
    [[nodiscard]] nlohmann::json get_evolution_history() const;
    [[nodiscard]] nlohmann::json get_agent_evolution_stats(const AgentId& agent_id) const;
    [[nodiscard]] nlohmann::json get_agent_runtime_config(const AgentId& agent_id) const;
    [[nodiscard]] nlohmann::json get_agent_runtime_version(const AgentId& agent_id) const;
    [[nodiscard]] nlohmann::json get_agent_runtime_history(const AgentId& agent_id) const;
    [[nodiscard]] nlohmann::json get_agent_runtime_diff(const AgentId& agent_id,
                                                        std::uint64_t from_version,
                                                        std::uint64_t to_version) const;
    bool rollback_proposal(const std::string& proposal_id, std::string* reason = nullptr);
    [[nodiscard]] static bool validate_patch_schema(const nlohmann::json& patch, std::string* reason = nullptr);
    [[nodiscard]] nlohmann::json list_rollback_snapshots() const;
    void save_snapshots() const;
    void load_snapshots();
    void clear_expired_snapshots(std::chrono::seconds max_age);
    void save_evolution_history() const;
    void load_evolution_history();
    void save_runtime_config_versions() const;
    void load_runtime_config_versions();
    void save_runtime_config_history() const;
    void load_runtime_config_history();
    void clear_old_evolution_history(std::size_t keep_last);
    void clear_old_runtime_config_history(std::size_t keep_last_per_agent);
    [[nodiscard]] bool verify_event_log() const;

private:
    Config config_;
    bool initialized_ = false;

    std::shared_ptr<protocol::MessageBus> bus_;
    std::unique_ptr<router::Router> router_;
    std::unique_ptr<memory::WorkingMemory> working_memory_;
    std::unique_ptr<memory::OrgLog> org_log_;
    std::unique_ptr<zone::ZoneManager> zone_manager_;
    std::unique_ptr<compiler::Compiler> compiler_;
    std::unique_ptr<entropy::EntropyMonitor> entropy_monitor_;
    std::unique_ptr<event_log::EventLog> event_log_;
    std::unique_ptr<governance::GovernanceKernel> governance_;
    std::unique_ptr<evolution::Evolver> evolver_;
    std::shared_ptr<llm::LLMClient> llm_client_;
    std::shared_ptr<budget::BudgetTracker> budget_tracker_;
    std::shared_ptr<slp::SLPCompressor> slp_compressor_;

    std::unordered_map<AgentId, std::shared_ptr<agent::Agent>> agents_;
    struct RollbackSnapshot {
        AgentId agent_id;
        nlohmann::json config_before;
        nlohmann::json config_after;
        Timestamp applied_at;
    };

    std::unordered_map<std::string, RollbackSnapshot> rollback_snapshots_;
    std::vector<nlohmann::json> evolution_history_;
    nlohmann::json last_evolution_report_;
    EventCallback event_callback_;
    mutable std::mutex event_callback_mutex_;

    static governance::Constitution load_constitution(const std::filesystem::path& path);
    [[nodiscard]] static nlohmann::json compute_runtime_config_diff(const nlohmann::json& before,
                                                                    const nlohmann::json& after);
    [[nodiscard]] std::optional<nlohmann::json> get_runtime_snapshot_for_version(const AgentId& agent_id,
                                                                                  std::uint64_t version) const;
    void record_runtime_config_version(const AgentId& agent_id,
                                       const std::string& proposal_id,
                                       const nlohmann::json& before,
                                       const nlohmann::json& after);
    void emit_event(nlohmann::json event);
    void run_evolution_cycle();
    void ensure_initialized() const;

    std::unordered_map<AgentId, std::uint64_t> runtime_config_versions_;
    std::vector<RuntimeConfigVersionRecord> runtime_config_history_;
};

} // namespace evoclaw::facade
