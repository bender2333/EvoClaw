#include "evolution/evolver.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace evoclaw::evolution {
namespace {

int estimate_word_tokens(const std::string& text) {
    std::istringstream iss(text);
    int count = 0;
    std::string token;
    while (iss >> token) {
        ++count;
    }
    return count;
}

int estimate_proposal_tokens(const Tension& tension) {
    const int description_tokens = estimate_word_tokens(tension.description);
    const int severity_tokens = estimate_word_tokens(tension.severity);
    return std::clamp(description_tokens + severity_tokens + 64, 64, 512);
}

} // namespace

Evolver::Evolver(governance::GovernanceKernel& governance)
    : config_(),
      governance_(governance),
      budget_(config_.max_evolution_cycles_per_hour, config_.evolution_token_budget) {}

Evolver::Evolver(governance::GovernanceKernel& governance, Config config)
    : config_(std::move(config)),
      governance_(governance),
      budget_(config_.max_evolution_cycles_per_hour, config_.evolution_token_budget) {}

std::vector<Tension> Evolver::monitor(const memory::OrgLog& log) {
    std::vector<Tension> tensions;

    if (!budget_.can_evolve()) {
        return tensions;
    }
    budget_.record_cycle();

    // 获取所有 agent 的统计
    const auto& entries = log.all_entries();
    if (entries.empty()) {
        return tensions;
    }

    // 按 agent 分组统计
    std::unordered_map<AgentId, std::vector<double>> agent_scores;
    std::unordered_map<AgentId, int> agent_failures;

    for (const auto& entry : entries) {
        agent_scores[entry.agent_id].push_back(entry.critic_score);
        if (entry.critic_score < 0.5) {
            agent_failures[entry.agent_id]++;
        }
    }

    // 检测 KPI 下降
    for (const auto& [agent_id, scores] : agent_scores) {
        if (scores.size() < 2) {
            continue;
        }

        double total = std::accumulate(scores.begin(), scores.end(), 0.0);
        double avg = total / static_cast<double>(scores.size());

        // 最近 N 个的平均 vs 整体平均
        std::size_t recent_n = std::min(scores.size() / 2, static_cast<std::size_t>(5));
        if (recent_n == 0) {
            continue;
        }

        double recent_total = 0.0;
        for (std::size_t i = scores.size() - recent_n; i < scores.size(); ++i) {
            recent_total += scores[i];
        }
        double recent_avg = recent_total / static_cast<double>(recent_n);

        if (avg > 0.0 && recent_avg < avg * config_.kpi_decline_threshold) {
            Tension t;
            t.id = generate_uuid();
            t.type = TensionType::KPI_DECLINE;
            t.source_agent = agent_id;
            t.description = "KPI decline detected: recent avg " +
                            std::to_string(recent_avg) + " < threshold " +
                            std::to_string(avg * config_.kpi_decline_threshold);
            t.severity = "high";
            t.metadata = {{"agent_id", agent_id}, {"recent_avg", recent_avg}, {"overall_avg", avg}};
            tensions.push_back(std::move(t));
        }
    }

    // 检测连续失败
    for (const auto& [agent_id, failures] : agent_failures) {
        if (failures >= config_.consecutive_failures) {
            Tension t;
            t.id = generate_uuid();
            t.type = TensionType::REPEATED_FAILURE;
            t.source_agent = agent_id;
            t.description = "Repeated failures: " + std::to_string(failures) + " failures detected";
            t.severity = "high";
            t.metadata = {{"agent_id", agent_id}, {"failure_count", failures}};
            tensions.push_back(std::move(t));
        }
    }

    return tensions;
}

std::vector<EvolutionProposal> Evolver::propose(const std::vector<Tension>& tensions) {
    std::vector<EvolutionProposal> proposals;

    if (tensions.empty()) {
        return proposals;
    }

    const std::size_t max_proposals = config_.max_proposals_per_cycle > 0
                                          ? static_cast<std::size_t>(config_.max_proposals_per_cycle)
                                          : tensions.size();

    for (const auto& tension : tensions) {
        if (proposals.size() >= max_proposals) {
            break;
        }

        const int estimated_tokens = estimate_proposal_tokens(tension);
        const nlohmann::json status = budget_.status();
        const int token_budget = status.value("token_budget", 0);
        const int remaining = status.value("tokens_remaining", -1);
        if (token_budget > 0 && remaining >= 0 && remaining < estimated_tokens) {
            break;
        }
        budget_.record_tokens(estimated_tokens);

        EvolutionProposal proposal;
        proposal.id = generate_uuid();
        proposal.target_agent = tension.source_agent;
        proposal.tension_ids.push_back(tension.id);

        switch (tension.type) {
            case TensionType::KPI_DECLINE:
                proposal.type = EvolutionType::ADJUSTMENT;
                proposal.description = "Adjust agent prompt to improve performance";
                proposal.rationale = "KPI decline detected: " + tension.description;
                break;
            case TensionType::REPEATED_FAILURE:
                proposal.type = EvolutionType::REPLACEMENT;
                proposal.description = "Replace or retrain agent due to repeated failures";
                proposal.rationale = "Too many failures: " + tension.description;
                break;
            case TensionType::REFLECTION_SUGGESTION:
                proposal.type = EvolutionType::ADJUSTMENT;
                proposal.description = "Apply reflection-based improvements";
                proposal.rationale = "Agent self-reflection suggested changes";
                break;
            case TensionType::MANUAL:
                proposal.type = EvolutionType::ADJUSTMENT;
                proposal.description = "Manual evolution trigger";
                proposal.rationale = tension.description;
                break;
        }

        proposals.push_back(std::move(proposal));
    }

    return proposals;
}

ABTestResult Evolver::run_ab_test(const EvolutionProposal& proposal,
                                  const std::vector<double>& control_scores,
                                  const std::vector<double>& candidate_scores) const {
    ABTestResult result;
    result.control_id = proposal.target_agent;
    result.candidate_id = proposal.target_agent + "_candidate";
    result.sample_size = static_cast<int>(std::min(control_scores.size(), candidate_scores.size()));

    if (control_scores.empty() || candidate_scores.empty()) {
        return result;
    }

    double control_sum = std::accumulate(control_scores.begin(), control_scores.end(), 0.0);
    double candidate_sum = std::accumulate(candidate_scores.begin(), candidate_scores.end(), 0.0);

    result.control_score = control_sum / static_cast<double>(control_scores.size());
    result.candidate_score = candidate_sum / static_cast<double>(candidate_scores.size());

    if (result.control_score > 0.0) {
        result.improvement = (result.candidate_score - result.control_score) / result.control_score;
    }

    // 简化的显著性检验：改进超过阈值且样本量足够
    result.significant = (result.improvement >= config_.min_improvement) && (result.sample_size >= 5);
    result.p_value = result.significant ? 0.01 : 0.5;

    return result;
}

bool Evolver::apply_evolution(const EvolutionProposal& proposal,
                              const ABTestResult& test_result) const {
    // 检查改进是否显著
    if (!test_result.significant || test_result.improvement < config_.min_improvement) {
        return false;
    }

    // 检查治理层是否允许
    nlohmann::json details = {
        {"proposal_id", proposal.id},
        {"improvement", test_result.improvement},
        {"p_value", test_result.p_value}
    };

    auto permission = governance_.evaluate_action(
        "evolver", proposal.description, details);

    return permission == governance::Permission::ALLOW;
}

bool Evolver::can_evolve() const {
    return budget_.can_evolve();
}

nlohmann::json Evolver::budget_status() const {
    nlohmann::json status = budget_.status();
    status["max_proposals_per_cycle"] = config_.max_proposals_per_cycle;
    status["evolution_token_budget"] = config_.evolution_token_budget;
    return status;
}

} // namespace evoclaw::evolution
