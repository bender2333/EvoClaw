#pragma once

#include "evolution/tension.hpp"
#include "governance/governance_kernel.hpp"
#include "memory/org_log.hpp"

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
    bool significant = false;
};

class Evolver {
public:
    struct Config {
        double kpi_decline_threshold = 0.8;
        int consecutive_failures = 10;
        double min_improvement = 0.05;
        double p_value_threshold = 0.05;
    };

    explicit Evolver(governance::GovernanceKernel& governance);
    Evolver(governance::GovernanceKernel& governance, Config config);

    [[nodiscard]] std::vector<Tension> monitor(const memory::OrgLog& log) const;
    [[nodiscard]] std::vector<EvolutionProposal> propose(const std::vector<Tension>& tensions) const;
    [[nodiscard]] ABTestResult run_ab_test(const EvolutionProposal& proposal,
                                           const std::vector<double>& control_scores,
                                           const std::vector<double>& candidate_scores) const;
    [[nodiscard]] bool apply_evolution(const EvolutionProposal& proposal,
                                       const ABTestResult& test_result) const;

private:
    Config config_;
    governance::GovernanceKernel& governance_;
};

} // namespace evoclaw::evolution
