#pragma once

#include "evolution/evolution_budget.hpp"
#include "evolution/tension.hpp"
#include "governance/governance_kernel.hpp"
#include "llm/llm_client.hpp"
#include "memory/org_log.hpp"

#include <memory>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace evoclaw::evolution {

enum class EvolutionType {
    ADJUSTMENT,
    REPLACEMENT,
    RESTRUCTURING
};

struct EvolutionProposal {
    std::string id;
    EvolutionType type = EvolutionType::ADJUSTMENT;
    AgentId target_agent;
    std::string description;
    nlohmann::json old_value = nlohmann::json::object();
    nlohmann::json new_value = nlohmann::json::object();
    std::string rationale;
    std::vector<std::string> tension_ids;
};

struct ABTestResult {
    AgentId control_id;
    AgentId candidate_id;
    double control_score = 0.0;
    double candidate_score = 0.0;
    double improvement = 0.0;
    int sample_size = 0;
    double p_value = 1.0;
    bool min_sample_met = false;
    double confidence = 0.0;
    bool significant = false;
};

class Evolver {
public:
    struct Config {
        double kpi_decline_threshold = 0.8;
        double ewma_decay = 0.2;
        double volatility_sensitivity = 0.2;
        int consecutive_failures = 10;
        double min_improvement = 0.05;
        int min_sample_size = 8;
        double confidence_threshold = 0.8;
        double p_value_threshold = 0.05;
        int max_evolution_cycles_per_hour = 5;
        int max_proposals_per_cycle = 3;
        int evolution_token_budget = 10000;
    };

    explicit Evolver(governance::GovernanceKernel& governance);
    Evolver(governance::GovernanceKernel& governance, Config config);

    void set_llm_client(std::shared_ptr<llm::LLMClient> client);

    [[nodiscard]] std::vector<Tension> monitor(const memory::OrgLog& log);
    [[nodiscard]] std::vector<EvolutionProposal> propose(const std::vector<Tension>& tensions);
    [[nodiscard]] ABTestResult run_ab_test(const EvolutionProposal& proposal,
                                           const std::vector<double>& control_scores,
                                           const std::vector<double>& candidate_scores) const;
    [[nodiscard]] bool apply_evolution(const EvolutionProposal& proposal,
                                       const ABTestResult& test_result) const;
    [[nodiscard]] bool can_evolve() const;
    [[nodiscard]] nlohmann::json budget_status() const;

private:
    Config config_;
    governance::GovernanceKernel& governance_;
    EvolutionBudget budget_;
    std::shared_ptr<llm::LLMClient> llm_client_;
};

} // namespace evoclaw::evolution
