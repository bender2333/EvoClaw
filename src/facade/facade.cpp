#include "facade/facade.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace evoclaw::facade {
namespace {

using json = nlohmann::json;
constexpr const char* kMatrixStateFileName = "capability_matrix.json";

std::filesystem::path matrix_state_path(const std::filesystem::path& log_dir) {
    return log_dir / kMatrixStateFileName;
}

std::string tension_type_to_string(const evolution::TensionType type) {
    switch (type) {
        case evolution::TensionType::KPI_DECLINE:
            return "kpi_decline";
        case evolution::TensionType::REPEATED_FAILURE:
            return "repeated_failure";
        case evolution::TensionType::REFLECTION_SUGGESTION:
            return "reflection_suggestion";
        case evolution::TensionType::MANUAL:
            return "manual";
    }
    return "unknown";
}

std::string evolution_type_to_string(const evolution::EvolutionType type) {
    switch (type) {
        case evolution::EvolutionType::ADJUSTMENT:
            return "adjustment";
        case evolution::EvolutionType::REPLACEMENT:
            return "replacement";
        case evolution::EvolutionType::RESTRUCTURING:
            return "restructuring";
    }
    return "unknown";
}

std::string task_type_to_string(const TaskType type) {
    switch (type) {
        case TaskType::ROUTE:
            return "route";
        case TaskType::EXECUTE:
            return "execute";
        case TaskType::CRITIQUE:
            return "critique";
        case TaskType::SYNTHESIZE:
            return "synthesize";
        case TaskType::EVOLVE:
            return "evolve";
    }
    return "unknown";
}

double extract_critic_score(const agent::Task& task, const agent::TaskResult& result) {
    if (result.metadata.contains("score") && result.metadata["score"].is_number()) {
        return std::clamp(result.metadata["score"].get<double>(), 0.0, 1.0);
    }
    if (task.context.contains("critic_score") && task.context["critic_score"].is_number()) {
        return std::clamp(task.context["critic_score"].get<double>(), 0.0, 1.0);
    }
    if (task.context.contains("score") && task.context["score"].is_number()) {
        return std::clamp(task.context["score"].get<double>(), 0.0, 1.0);
    }
    return std::clamp(result.confidence, 0.0, 1.0);
}

json make_event_details(const agent::Task& task,
                        const agent::TaskResult& result,
                        const double duration_ms,
                        const double critic_score) {
    return {
        {"task_type", static_cast<int>(task.type)},
        {"description", task.description},
        {"context", task.context},
        {"success", result.success},
        {"confidence", result.confidence},
        {"critic_score", critic_score},
        {"duration_ms", duration_ms},
        {"result_metadata", result.metadata}
    };
}

} // namespace

EvoClawFacade::EvoClawFacade() : config_() {}

EvoClawFacade::EvoClawFacade(Config config)
    : config_(std::move(config)) {}

void EvoClawFacade::initialize() {
    std::error_code ec;
    std::filesystem::create_directories(config_.log_dir, ec);

    bus_ = std::make_shared<protocol::MessageBus>();
    router_ = std::make_unique<router::Router>(config_.router_config);
    router_->load_matrix(matrix_state_path(config_.log_dir));
    org_log_ = std::make_unique<memory::OrgLog>(config_.log_dir);
    zone_manager_ = std::make_unique<zone::ZoneManager>();
    router_->set_zone_manager(zone_manager_.get());
    compiler_ = std::make_unique<compiler::Compiler>(*org_log_);
    entropy_monitor_ = std::make_unique<entropy::EntropyMonitor>(*router_);
    event_log_ = std::make_unique<event_log::EventLog>(config_.log_dir / "event_log.jsonl");

    governance_ = std::make_unique<governance::GovernanceKernel>(load_constitution(config_.config_path));
    llm_client_ = std::make_shared<llm::LLMClient>(llm::create_from_env());
    evolver_ = std::make_unique<evolution::Evolver>(*governance_, config_.evolver_config);
    evolver_->set_llm_client(llm_client_);
    budget_tracker_ = std::make_shared<budget::BudgetTracker>();

    last_evolution_report_ = {
        {"status", "not_run"},
        {"tension_count", 0},
        {"proposal_count", 0},
        {"applied_count", 0}
    };

    initialized_ = true;
}

void EvoClawFacade::set_event_callback(EventCallback callback) {
    std::lock_guard<std::mutex> lock(event_callback_mutex_);
    event_callback_ = std::move(callback);
}

void EvoClawFacade::register_agent(std::shared_ptr<agent::Agent> agent) {
    register_agent(std::move(agent), zone::Zone::STABLE);
}

void EvoClawFacade::register_agent(std::shared_ptr<agent::Agent> agent, const zone::Zone zone) {
    ensure_initialized();
    if (!agent) {
        throw std::invalid_argument("register_agent requires a non-null agent");
    }

    agent->set_message_bus(bus_);
    if (llm_client_) {
        agent->set_llm_client(llm_client_);
    }
    bus_->subscribe(agent->id(), [weak_agent = std::weak_ptr<agent::Agent>(agent)](const protocol::Message& msg) {
        if (const auto locked = weak_agent.lock()) {
            locked->on_message(msg);
        }
    });

    router_->register_agent(agent);
    const auto agent_id = agent->id();
    if (zone_manager_) {
        zone_manager_->assign_zone(agent_id, zone);
    }
    agents_[agent_id] = std::move(agent);

    event_log::Event event;
    event.type = event_log::EventType::AGENT_SPAWN;
    event.actor = "facade";
    event.target = agent_id;
    event.action = "register_agent";
    event.details = {
        {"agent_count", agents_.size()},
        {"subscriber_count", bus_->subscriber_count()},
        {"zone", zone::zone_to_string(zone)}
    };
    event_log_->append(std::move(event));

    emit_event({
        {"event", "agent_registered"},
        {"agent_id", agent_id},
        {"zone", zone::zone_to_string(zone)},
        {"agent_count", agents_.size()},
        {"subscriber_count", bus_->subscriber_count()},
        {"message", "Agent " + agent_id + " registered"}
    });
}

agent::TaskResult EvoClawFacade::submit_task(const agent::Task& task) {
    ensure_initialized();

    emit_event({
        {"event", "task_submitted"},
        {"task_id", task.id},
        {"description", task.description},
        {"task_type", task_type_to_string(task.type)},
        {"context", task.context},
        {"message", "Task " + task.id + " submitted"}
    });

    agent::TaskResult failed;
    failed.task_id = task.id;
    failed.success = false;

    const auto find_agent_by_role = [this](const std::string& expected_role) {
        for (const auto& [_, agent_ptr] : agents_) {
            if (agent_ptr && agent_ptr->role() == expected_role) {
                return agent_ptr;
            }
        }
        return std::shared_ptr<agent::Agent>{};
    };

    const auto planner_agent = find_agent_by_role("planner");
    const auto executor_agent = find_agent_by_role("executor");
    const auto critic_agent = find_agent_by_role("critic");
    const bool use_pipeline = task.type == TaskType::EXECUTE && static_cast<bool>(executor_agent);

    if (use_pipeline) {
        const int stage_count = 1 + (planner_agent ? 1 : 0) + (critic_agent ? 1 : 0);
        if (task.budget.max_rounds > 0 && stage_count > task.budget.max_rounds) {
            failed.output = "Budget exceeded: required pipeline rounds exceed max_rounds";
            failed.metadata = {
                {"reason", "budget_rounds_exceeded"},
                {"required_rounds", stage_count},
                {"max_rounds", task.budget.max_rounds}
            };
            return failed;
        }
        if (task.budget.max_tool_calls > 0 && stage_count > task.budget.max_tool_calls) {
            failed.output = "Budget exceeded: required pipeline calls exceed max_tool_calls";
            failed.metadata = {
                {"reason", "budget_tool_calls_exceeded"},
                {"required_calls", stage_count},
                {"max_tool_calls", task.budget.max_tool_calls}
            };
            return failed;
        }

        const int per_stage_max_tokens =
            task.budget.max_tokens > 0
                ? std::max(1, task.budget.max_tokens / std::max(1, stage_count))
                : task.budget.max_tokens;

        auto stage_task_template = task;
        stage_task_template.context["internal_call"] = true;

        auto stage_budget = task.budget;
        if (per_stage_max_tokens > 0) {
            stage_budget.max_tokens = per_stage_max_tokens;
        }
        if (stage_budget.max_rounds > 0) {
            stage_budget.max_rounds = 1;
        }
        if (stage_budget.max_tool_calls > 0) {
            stage_budget.max_tool_calls = std::max(1, stage_budget.max_tool_calls / std::max(1, stage_count));
        }

        const auto pipeline_start = std::chrono::steady_clock::now();
        nlohmann::json pipeline_stages = nlohmann::json::array();

        auto execute_stage = [&](const std::shared_ptr<agent::Agent>& stage_agent,
                                 const std::string& stage_name,
                                 const nlohmann::json& stage_context) {
            agent::Task stage_task = stage_task_template;
            stage_task.budget = stage_budget;
            stage_task.context["pipeline_stage"] = stage_name;
            for (const auto& [key, value] : stage_context.items()) {
                stage_task.context[key] = value;
            }

            const auto stage_begin = std::chrono::steady_clock::now();
            agent::TaskResult stage_result = stage_agent->execute(stage_task);
            const auto stage_end = std::chrono::steady_clock::now();

            if (budget_tracker_) {
                budget_tracker_->record(
                    stage_agent->id(),
                    stage_agent->prompt_tokens_consumed(),
                    stage_agent->completion_tokens_consumed());
            }
            if (zone_manager_) {
                zone_manager_->record_result(stage_agent->id(), stage_result.success);
                if (zone_manager_->is_promotion_eligible(stage_agent->id())) {
                    zone_manager_->promote_to_stable(stage_agent->id());
                }
            }

            const double stage_duration_ms =
                static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(stage_end - stage_begin)
                                        .count());
            pipeline_stages.push_back({
                {"stage", stage_name},
                {"agent_id", stage_agent->id()},
                {"success", stage_result.success},
                {"confidence", stage_result.confidence},
                {"duration_ms", stage_duration_ms},
                {"metadata", stage_result.metadata}
            });

            return stage_result;
        };

        std::string plan_output;
        if (planner_agent) {
            const auto planner_result = execute_stage(
                planner_agent,
                "plan",
                {{"intent", "plan"}, {"task_rounds", 1}, {"required_permissions", nlohmann::json::array()}});
            if (planner_result.success) {
                plan_output = planner_result.output;
            }
        }

        const auto executor_result = execute_stage(
            executor_agent,
            "execute",
            {{"intent", "execute"},
             {"plan", plan_output},
             {"task_rounds", 1},
             {"required_permissions", nlohmann::json::array()}});

        agent::TaskResult result = executor_result;
        if (result.agent_id.empty()) {
            result.agent_id = executor_agent->id();
        }

        double critic_score = extract_critic_score(task, result);
        if (critic_agent) {
            nlohmann::json critique_context = {
                {"intent", "critique"},
                {"target_agent", executor_agent->id()},
                {"result_output", executor_result.output},
                {"result_confidence", executor_result.confidence},
                {"success", executor_result.success},
                {"task_rounds", 1},
                {"required_permissions", nlohmann::json::array()}
            };
            const auto critique_result = execute_stage(critic_agent, "critique", critique_context);
            if (critique_result.success) {
                critic_score = extract_critic_score(task, critique_result);
                result.metadata["critic"] = critique_result.metadata;
                result.metadata["critic_output"] = critique_result.output;

                const std::string verdict =
                    critique_result.metadata.value("verdict", std::string("accept"));
                if (verdict == "reject" || critic_score < 0.7) {
                    result.success = false;
                }
            }
        }

        const auto pipeline_end = std::chrono::steady_clock::now();
        const double total_duration_ms =
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(pipeline_end - pipeline_start).count());

        result.metadata["pipeline"] = {
            {"enabled", true},
            {"stage_count", stage_count},
            {"stages", pipeline_stages},
            {"budget", {
                {"max_tokens", task.budget.max_tokens},
                {"max_tool_calls", task.budget.max_tool_calls},
                {"max_rounds", task.budget.max_rounds},
                {"per_stage_max_tokens", per_stage_max_tokens}
            }}
        };

        memory::OrgLogEntry org_entry;
        org_entry.task_id = task.id;
        org_entry.agent_id = result.agent_id;
        org_entry.module_version = executor_agent->contract().version;
        org_entry.duration_ms = total_duration_ms;
        org_entry.critic_score = critic_score;
        org_entry.user_feedback_positive = critic_score >= 0.7;
        org_entry.metadata = {
            {"description", task.description},
            {"context", task.context},
            {"effective_budget_max_tokens", task.budget.max_tokens},
            {"success", result.success},
            {"confidence", result.confidence},
            {"result_metadata", result.metadata}
        };
        org_log_->append(org_entry);

        event_log::Event event;
        event.type = result.success ? event_log::EventType::TASK_COMPLETE : event_log::EventType::TASK_FAILED;
        event.actor = org_entry.agent_id;
        event.target = task.id;
        event.action = result.success ? "task_completed" : "task_failed";
        event.details = make_event_details(task, result, total_duration_ms, critic_score);
        event_log_->append(std::move(event));

        emit_event({
            {"event", result.success ? "task_complete" : "task_failed"},
            {"task_id", task.id},
            {"agent_id", org_entry.agent_id},
            {"success", result.success},
            {"confidence", result.confidence},
            {"critic_score", critic_score},
            {"duration_ms", total_duration_ms},
            {"metadata", result.metadata},
            {"message", result.success ? "Task " + task.id + " completed"
                                        : "Task " + task.id + " failed"}
        });

        return result;
    }

    const auto routed_agent = router_->route(task);
    if (!routed_agent.has_value()) {
        failed.output = "No agent available for task";
        failed.metadata = {{"reason", "router_no_match"}};

        event_log::Event event;
        event.type = event_log::EventType::TASK_FAILED;
        event.actor = "router";
        event.target = task.id;
        event.action = "route_failed";
        event.details = {{"description", task.description}, {"context", task.context}};
        event_log_->append(std::move(event));

        emit_event({
            {"event", "task_failed"},
            {"task_id", task.id},
            {"description", task.description},
            {"reason", "router_no_match"},
            {"message", "Task " + task.id + " failed: no agent available"}
        });
        return failed;
    }

    const auto agent_it = agents_.find(*routed_agent);
    if (agent_it == agents_.end() || !agent_it->second) {
        failed.output = "Routed agent is not registered";
        failed.metadata = {{"agent_id", *routed_agent}, {"reason", "agent_missing"}};

        event_log::Event event;
        event.type = event_log::EventType::TASK_FAILED;
        event.actor = "router";
        event.target = task.id;
        event.action = "agent_missing";
        event.details = {{"agent_id", *routed_agent}};
        event_log_->append(std::move(event));

        emit_event({
            {"event", "task_failed"},
            {"task_id", task.id},
            {"agent_id", *routed_agent},
            {"reason", "agent_missing"},
            {"message", "Task " + task.id + " failed: routed agent missing"}
        });
        return failed;
    }

    agent::Task effective_task = task;
    if (zone_manager_) {
        const zone::ZonePolicy policy = zone_manager_->get_policy(agent_it->second->id());
        if (policy.token_budget_ratio > 0.0 &&
            policy.token_budget_ratio < 1.0 &&
            effective_task.budget.max_tokens > 0) {
            const double scaled_tokens = std::floor(static_cast<double>(effective_task.budget.max_tokens) *
                                                    policy.token_budget_ratio);
            effective_task.budget.max_tokens = std::max(1, static_cast<int>(scaled_tokens));
        }
    }

    const auto start = std::chrono::steady_clock::now();
    agent::TaskResult result = agent_it->second->execute(effective_task);
    const auto end = std::chrono::steady_clock::now();

    if (zone_manager_) {
        zone_manager_->record_result(agent_it->second->id(), result.success);
        if (zone_manager_->is_promotion_eligible(agent_it->second->id())) {
            zone_manager_->promote_to_stable(agent_it->second->id());
        }
    }

    if (budget_tracker_) {
        budget_tracker_->record(
            agent_it->second->id(),
            agent_it->second->prompt_tokens_consumed(),
            agent_it->second->completion_tokens_consumed());
    }

    const double duration_ms =
        static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    const double critic_score = extract_critic_score(task, result);

    memory::OrgLogEntry org_entry;
    org_entry.task_id = task.id;
    org_entry.agent_id = result.agent_id.empty() ? agent_it->second->id() : result.agent_id;
    org_entry.module_version = agent_it->second->contract().version;
    org_entry.duration_ms = duration_ms;
    org_entry.critic_score = critic_score;
    org_entry.user_feedback_positive = critic_score >= 0.7;
    org_entry.metadata = {
        {"description", task.description},
        {"context", task.context},
        {"effective_budget_max_tokens", effective_task.budget.max_tokens},
        {"success", result.success},
        {"confidence", result.confidence},
        {"result_metadata", result.metadata}
    };
    org_log_->append(org_entry);

    event_log::Event event;
    event.type = result.success ? event_log::EventType::TASK_COMPLETE : event_log::EventType::TASK_FAILED;
    event.actor = org_entry.agent_id;
    event.target = task.id;
    event.action = result.success ? "task_completed" : "task_failed";
    event.details = make_event_details(task, result, duration_ms, critic_score);
    event_log_->append(std::move(event));

    emit_event({
        {"event", result.success ? "task_complete" : "task_failed"},
        {"task_id", task.id},
        {"agent_id", org_entry.agent_id},
        {"success", result.success},
        {"confidence", result.confidence},
        {"critic_score", critic_score},
        {"duration_ms", duration_ms},
        {"metadata", result.metadata},
        {"message", result.success ? "Task " + task.id + " completed" : "Task " + task.id + " failed"}
    });

    return result;
}

void EvoClawFacade::save_state() const {
    ensure_initialized();
    router_->save_matrix(matrix_state_path(config_.log_dir));
}

void EvoClawFacade::trigger_evolution() {
    ensure_initialized();
    run_evolution_cycle();
    save_state();
}

std::vector<memory::OrgLogEntry> EvoClawFacade::query_logs(Timestamp start, Timestamp end) const {
    ensure_initialized();
    return org_log_->query_by_time_range(start, end);
}

memory::OrgLog::TimeRangeStats EvoClawFacade::get_log_stats(Timestamp start, Timestamp end) const {
    ensure_initialized();
    return org_log_->get_stats_for_range(start, end);
}

nlohmann::json EvoClawFacade::get_status() const {
    const std::size_t org_entries = (initialized_ && org_log_) ? org_log_->all_entries().size() : 0U;

    return {
        {"initialized", initialized_},
        {"log_dir", config_.log_dir.string()},
        {"config_path", config_.config_path.string()},
        {"agent_count", agents_.size()},
        {"agents", get_capability_matrix().value("agents", json::array())},
        {"bus_subscribers", bus_ ? bus_->subscriber_count() : 0U},
        {"llm", llm_client_ ? llm_client_->status_json()
                             : json{
                                 {"base_url", ""},
                                 {"model", ""},
                                 {"has_api_key", false},
                                 {"mock_mode", true},
                                 {"temperature", 0.0},
                                 {"max_tokens", 0}
                             }},
        {"org_log_entries", org_entries},
        {"event_log_integrity", (event_log_ ? event_log_->verify_integrity() : true)},
        {"evolution", {{"last_cycle", last_evolution_report_}}},
        {"zones", get_zone_status()},
        {"patterns", get_pattern_status()},
        {"entropy", get_entropy_status()}
    };
}

nlohmann::json EvoClawFacade::get_capability_matrix() const {
    json matrix;
    matrix["agents"] = json::array();
    matrix["by_intent"] = json::object();

    for (const auto& [agent_id, agent_ptr] : agents_) {
        if (!agent_ptr) {
            continue;
        }

        const auto& contract = agent_ptr->contract();
        matrix["agents"].push_back({
            {"agent_id", agent_id},
            {"role", agent_ptr->role()},
            {"module_id", contract.module_id},
            {"version", contract.version},
            {"intent_tags", contract.capability.intent_tags},
            {"required_tools", contract.capability.required_tools},
            {"success_rate_threshold", contract.capability.success_rate_threshold},
            {"estimated_cost_token", contract.capability.estimated_cost_token}
        });

        for (const auto& intent : contract.capability.intent_tags) {
            matrix["by_intent"][intent].push_back({
                {"agent_id", agent_id},
                {"role", agent_ptr->role()},
                {"score", contract.capability.success_rate_threshold}
            });
        }
    }

    return matrix;
}

governance::Constitution EvoClawFacade::load_constitution(const std::filesystem::path& path) {
    governance::Constitution constitution;
    constitution.mission = "EvoClaw default mission";
    constitution.values = {"safety", "quality"};
    constitution.guardrails = {};
    constitution.evolution_policy = {{"deployment_score_threshold", 0.8}};

    std::ifstream in(path);
    if (!in.is_open()) {
        return constitution;
    }

    json parsed;
    try {
        in >> parsed;
    } catch (...) {
        return constitution;
    }

    if (!parsed.is_object()) {
        return constitution;
    }

    constitution.mission = parsed.value("mission", constitution.mission);
    constitution.values = parsed.value("values", constitution.values);
    constitution.guardrails = parsed.value("guardrails", constitution.guardrails);
    if (parsed.contains("evolution_policy") && parsed["evolution_policy"].is_object()) {
        constitution.evolution_policy = parsed["evolution_policy"];
    }

    return constitution;
}

void EvoClawFacade::run_evolution_cycle() {
    const auto anti_entropy_actions = entropy_monitor_
                                          ? entropy_monitor_->suggest_actions()
                                          : std::vector<entropy::AntiEntropyAction>{};
    if (entropy_monitor_ && router_) {
        for (const auto& action : anti_entropy_actions) {
            if (action.type == "cold_start_perturbation") {
                entropy_monitor_->apply_cold_start_perturbation(*router_, 0.05);
            }
        }
    }

    emit_event({
        {"event", "evolution_started"},
        {"message", "Evolution cycle started"}
    });

    const std::vector<evolution::Tension> tensions = evolver_->monitor(*org_log_);
    const std::vector<evolution::EvolutionProposal> proposals = evolver_->propose(tensions);

    json report = {
        {"status", "completed"},
        {"tension_count", tensions.size()},
        {"proposal_count", proposals.size()},
        {"applied_count", 0},
        {"anti_entropy_actions", json::array()},
        {"tensions", json::array()},
        {"proposals", json::array()}
    };

    for (const auto& action : anti_entropy_actions) {
        report["anti_entropy_actions"].push_back({
            {"type", action.type},
            {"description", action.description},
            {"details", action.details}
        });
    }

    for (const auto& tension : tensions) {
        report["tensions"].push_back({
            {"id", tension.id},
            {"type", tension_type_to_string(tension.type)},
            {"source_agent", tension.source_agent},
            {"description", tension.description},
            {"severity", tension.severity},
            {"metadata", tension.metadata}
        });

        event_log::Event tension_event;
        tension_event.type = event_log::EventType::EVOLUTION;
        tension_event.actor = "evolver";
        tension_event.target = tension.source_agent;
        tension_event.action = "tension_detected";
        tension_event.details = {
            {"tension_id", tension.id},
            {"type", tension_type_to_string(tension.type)},
            {"description", tension.description},
            {"severity", tension.severity}
        };
        event_log_->append(std::move(tension_event));

        emit_event({
            {"event", "tension_detected"},
            {"tension_id", tension.id},
            {"type", tension_type_to_string(tension.type)},
            {"source_agent", tension.source_agent},
            {"description", tension.description},
            {"severity", tension.severity},
            {"message", "Tension detected: " + tension.description}
        });
    }

    for (const auto& proposal : proposals) {
        std::vector<memory::OrgLogEntry> entries = org_log_->query_by_agent(proposal.target_agent, 50);
        std::vector<double> control_scores;
        control_scores.reserve(entries.size());
        for (const auto& entry : entries) {
            control_scores.push_back(std::clamp(entry.critic_score, 0.0, 1.0));
        }

        if (control_scores.empty()) {
            control_scores = {0.6, 0.6, 0.6, 0.6, 0.6};
        }
        while (control_scores.size() < 8U) {
            control_scores.push_back(control_scores.back());
        }

        std::vector<double> candidate_scores;
        candidate_scores.reserve(control_scores.size());
        for (const double control : control_scores) {
            candidate_scores.push_back(std::clamp(control + 0.1, 0.0, 1.0));
        }

        const auto test_result = evolver_->run_ab_test(proposal, control_scores, candidate_scores);
        bool applied = evolver_->apply_evolution(proposal, test_result);

        const bool rollback_triggered =
            governance_ && governance_->should_rollback(
                proposal.target_agent, test_result.candidate_score, test_result.control_score);

        const bool requires_confirmation =
            proposal.type == evolution::EvolutionType::REPLACEMENT ||
            proposal.type == evolution::EvolutionType::RESTRUCTURING ||
            (proposal.new_value.is_object() && proposal.new_value.value("high_risk", false));

        nlohmann::json config_before = nlohmann::json::object();
        nlohmann::json config_after = nlohmann::json::object();
        bool runtime_patch_applied = false;

        std::string rejection_reason = "none";
        if (applied) {
            const auto target_it = agents_.find(proposal.target_agent);
            if (target_it == agents_.end() || !target_it->second) {
                applied = false;
                rejection_reason = "target_agent_missing";
            } else {
                const auto patch_payload =
                    (proposal.new_value.is_object() && proposal.new_value.contains("patch"))
                        ? proposal.new_value["patch"]
                        : proposal.new_value;

                config_before = target_it->second->runtime_config();

                std::string patch_error;
                if (!target_it->second->apply_runtime_patch(patch_payload, &patch_error)) {
                    applied = false;
                    rejection_reason = patch_error.empty() ? "patch_apply_failed"
                                                           : ("patch_apply_failed:" + patch_error);
                } else {
                    runtime_patch_applied = true;
                    config_after = target_it->second->runtime_config();

                    event_log::Event config_event;
                    config_event.type = event_log::EventType::CONFIG_CHANGE;
                    config_event.actor = "evolver";
                    config_event.target = proposal.target_agent;
                    config_event.action = "runtime_patch_applied";
                    config_event.details = {
                        {"proposal_id", proposal.id},
                        {"before", config_before},
                        {"after", config_after}
                    };
                    event_log_->append(std::move(config_event));
                }
            }
        } else {
            if (!test_result.min_sample_met) {
                rejection_reason = "insufficient_sample";
            } else if (!test_result.significant) {
                rejection_reason = "not_significant";
            } else if (rollback_triggered) {
                rejection_reason = "rollback_guard";
            } else if (requires_confirmation) {
                rejection_reason = "requires_confirmation";
            } else {
                rejection_reason = "governance_denied";
            }
        }

        if (applied) {
            report["applied_count"] = report["applied_count"].get<int>() + 1;
        }

        report["proposals"].push_back({
            {"id", proposal.id},
            {"type", evolution_type_to_string(proposal.type)},
            {"target_agent", proposal.target_agent},
            {"description", proposal.description},
            {"rationale", proposal.rationale},
            {"tension_ids", proposal.tension_ids},
            {"new_value", proposal.new_value},
            {"ab_test", {
                {"control_score", test_result.control_score},
                {"candidate_score", test_result.candidate_score},
                {"improvement", test_result.improvement},
                {"sample_size", test_result.sample_size},
                {"min_sample_met", test_result.min_sample_met},
                {"p_value", test_result.p_value},
                {"confidence", test_result.confidence},
                {"significant", test_result.significant}
            }},
            {"requires_confirmation", requires_confirmation},
            {"rollback_triggered", rollback_triggered},
            {"runtime_patch_applied", runtime_patch_applied},
            {"config_before", config_before},
            {"config_after", config_after},
            {"rejection_reason", rejection_reason},
            {"applied", applied}
        });

        event_log::Event proposal_event;
        proposal_event.type = event_log::EventType::EVOLUTION;
        proposal_event.actor = "evolver";
        proposal_event.target = proposal.target_agent;
        proposal_event.action = applied ? "proposal_applied" : "proposal_rejected";
        proposal_event.details = {
            {"proposal_id", proposal.id},
            {"type", evolution_type_to_string(proposal.type)},
            {"improvement", test_result.improvement},
            {"sample_size", test_result.sample_size},
            {"min_sample_met", test_result.min_sample_met},
            {"significant", test_result.significant},
            {"p_value", test_result.p_value},
            {"confidence", test_result.confidence},
            {"requires_confirmation", requires_confirmation},
            {"rollback_triggered", rollback_triggered},
            {"runtime_patch_applied", runtime_patch_applied},
            {"rejection_reason", rejection_reason}
        };
        event_log_->append(std::move(proposal_event));

        if (rollback_triggered) {
            event_log::Event rollback_event;
            rollback_event.type = event_log::EventType::ROLLBACK;
            rollback_event.actor = "governance";
            rollback_event.target = proposal.target_agent;
            rollback_event.action = "rollback_triggered";
            rollback_event.details = {
                {"proposal_id", proposal.id},
                {"control_score", test_result.control_score},
                {"candidate_score", test_result.candidate_score},
                {"improvement", test_result.improvement}
            };
            event_log_->append(std::move(rollback_event));
        }

        emit_event({
            {"event", applied ? "proposal_applied" : "proposal_rejected"},
            {"proposal_id", proposal.id},
            {"type", evolution_type_to_string(proposal.type)},
            {"target_agent", proposal.target_agent},
            {"improvement", test_result.improvement},
            {"sample_size", test_result.sample_size},
            {"min_sample_met", test_result.min_sample_met},
            {"significant", test_result.significant},
            {"p_value", test_result.p_value},
            {"confidence", test_result.confidence},
            {"requires_confirmation", requires_confirmation},
            {"rollback_triggered", rollback_triggered},
            {"runtime_patch_applied", runtime_patch_applied},
            {"rejection_reason", rejection_reason},
            {"message", applied ? ("Proposal " + proposal.id + " applied")
                                : ("Proposal " + proposal.id + " rejected: " + rejection_reason)}
        });
    }

    last_evolution_report_ = std::move(report);

    emit_event({
        {"event", "evolution_completed"},
        {"tension_count", last_evolution_report_.value("tension_count", 0)},
        {"proposal_count", last_evolution_report_.value("proposal_count", 0)},
        {"applied_count", last_evolution_report_.value("applied_count", 0)},
        {"tensions", last_evolution_report_.value("tensions", json::array())},
        {"proposals", last_evolution_report_.value("proposals", json::array())},
        {"message", "Evolution cycle completed"}
    });
}

void EvoClawFacade::emit_event(json event) {
    EventCallback callback;
    {
        std::lock_guard<std::mutex> lock(event_callback_mutex_);
        callback = event_callback_;
    }

    if (!callback) {
        return;
    }

    if (!event.contains("timestamp")) {
        event["timestamp"] = timestamp_to_string(now());
    }

    try {
        callback(event);
    } catch (...) {
        // Callback exceptions are ignored to keep facade behavior stable.
    }
}

void EvoClawFacade::ensure_initialized() const {
    if (!initialized_ || !bus_ || !router_ || !org_log_ || !zone_manager_ || !compiler_ ||
        !entropy_monitor_ || !event_log_ || !governance_ || !evolver_) {
        throw std::runtime_error("EvoClawFacade is not initialized");
    }
}

nlohmann::json EvoClawFacade::get_agent_runtime_config(const AgentId& agent_id) const {
    const auto it = agents_.find(agent_id);
    if (it == agents_.end() || !it->second) {
        return nlohmann::json{{"error", "agent_not_found"}, {"agent_id", agent_id}};
    }
    return it->second->runtime_config();
}

bool EvoClawFacade::verify_event_log() const {
    if (!event_log_) return true;
    return event_log_->verify_integrity();
}

nlohmann::json EvoClawFacade::get_budget_report() const {
    if (!budget_tracker_) return nlohmann::json{{"error", "budget tracker not initialized"}};
    return budget_tracker_->report();
}

nlohmann::json EvoClawFacade::get_evolution_budget_status() const {
    if (!evolver_) return nlohmann::json{{"error", "evolver not initialized"}};
    // Check if evolver has evolution budget info in status
    auto status = get_status();
    if (status.contains("evolution")) {
        return status["evolution"];
    }
    return nlohmann::json{{"status", "no evolution data"}};
}

nlohmann::json EvoClawFacade::get_zone_status() const {
    if (!zone_manager_) {
        return nlohmann::json{{"error", "zone manager not initialized"}};
    }
    return zone_manager_->status();
}

nlohmann::json EvoClawFacade::get_pattern_status() const {
    if (!compiler_) {
        return nlohmann::json{{"error", "compiler not initialized"}};
    }

    const auto detected = compiler_->detect_patterns();
    for (const auto& pattern : detected) {
        const auto compiled_pattern = compiler_->compile(pattern);
        (void)compiled_pattern;
    }
    return compiler_->status();
}

nlohmann::json EvoClawFacade::get_entropy_status() const {
    if (!entropy_monitor_) {
        return nlohmann::json{{"error", "entropy monitor not initialized"}};
    }
    return entropy_monitor_->status();
}

} // namespace evoclaw::facade
