#include "evolution/evolver.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace evoclaw::evolution {
namespace {

struct EwmaStats {
    double score = 0.0;
    double volatility = 0.0;
};

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

std::string truncate_text(const std::string& text, std::size_t limit) {
    if (text.size() <= limit) {
        return text;
    }
    if (limit <= 3) {
        return text.substr(0, limit);
    }
    return text.substr(0, limit - 3) + "...";
}

std::optional<nlohmann::json> parse_json_object(const std::string& text) {
    try {
        const auto parsed = nlohmann::json::parse(text);
        if (parsed.is_object()) {
            return parsed;
        }
    } catch (...) {
    }

    const std::size_t begin = text.find('{');
    const std::size_t end = text.rfind('}');
    if (begin == std::string::npos || end == std::string::npos || end < begin) {
        return std::nullopt;
    }

    try {
        const auto parsed = nlohmann::json::parse(text.substr(begin, end - begin + 1U));
        if (parsed.is_object()) {
            return parsed;
        }
    } catch (...) {
    }

    return std::nullopt;
}

EwmaStats compute_ewma_stats(const std::vector<double>& scores, const double decay) {
    EwmaStats stats;
    if (scores.empty()) {
        return stats;
    }

    const double clamped_decay = std::clamp(decay, 0.01, 0.99);
    stats.score = std::clamp(scores.front(), 0.0, 1.0);
    stats.volatility = 0.0;

    for (std::size_t i = 1; i < scores.size(); ++i) {
        const double score = std::clamp(scores[i], 0.0, 1.0);
        const double delta = std::abs(score - stats.score);
        stats.score = clamped_decay * score + (1.0 - clamped_decay) * stats.score;
        stats.volatility = clamped_decay * delta + (1.0 - clamped_decay) * stats.volatility;
    }

    return stats;
}

double sample_variance(const std::vector<double>& values) {
    if (values.size() < 2U) {
        return 0.0;
    }

    const double mean = std::accumulate(values.begin(), values.end(), 0.0) /
                        static_cast<double>(values.size());
    double squared = 0.0;
    for (double value : values) {
        const double delta = value - mean;
        squared += delta * delta;
    }
    return squared / static_cast<double>(values.size() - 1U);
}

double normal_cdf(const double value) {
    return 0.5 * std::erfc(-value / std::sqrt(2.0));
}

double two_sided_p_value_from_z(const double z) {
    const double cdf = normal_cdf(std::abs(z));
    return std::clamp(2.0 * (1.0 - cdf), 0.0, 1.0);
}

void assign_default_proposal_fields(const Tension& tension, EvolutionProposal* proposal) {
    if (proposal == nullptr) {
        return;
    }

    switch (tension.type) {
        case TensionType::KPI_DECLINE:
            proposal->type = EvolutionType::ADJUSTMENT;
            proposal->description = "Adjust agent prompt to improve performance";
            proposal->rationale = "KPI decline detected: " + tension.description;
            proposal->new_value = {
                {"action", "update_prompt"},
                {"patch", {
                    {"system_prompt_suffix", "Prioritize accuracy, grounding, and concise verification steps."}
                }}
            };
            break;
        case TensionType::REPEATED_FAILURE:
            proposal->type = EvolutionType::REPLACEMENT;
            proposal->description = "Replace or retrain agent due to repeated failures";
            proposal->rationale = "Too many failures: " + tension.description;
            proposal->new_value = {
                {"action", "retrain_or_replace"},
                {"patch", {
                    {"temperature", 0.2},
                    {"retry_policy", "strict_backoff"}
                }}
            };
            break;
        case TensionType::REFLECTION_SUGGESTION:
            proposal->type = EvolutionType::ADJUSTMENT;
            proposal->description = "Apply reflection-based improvements";
            proposal->rationale = "Agent self-reflection suggested changes";
            proposal->new_value = {
                {"action", "apply_reflection_patch"},
                {"patch", {
                    {"enable_reflection_feedback", true}
                }}
            };
            break;
        case TensionType::MANUAL:
            proposal->type = EvolutionType::ADJUSTMENT;
            proposal->description = "Manual evolution trigger";
            proposal->rationale = tension.description;
            proposal->new_value = {
                {"action", "manual_adjustment"},
                {"patch", nlohmann::json::object()}
            };
            break;
    }
}

bool enrich_proposal_with_llm(const Tension& tension,
                              llm::LLMClient* llm_client,
                              EvolutionProposal* proposal) {
    if (llm_client == nullptr || proposal == nullptr) {
        return false;
    }

    const std::string system_prompt =
        "You are an evolution planner for a multi-agent system. "
        "Return only JSON object with fields: description (string), rationale (string), "
        "new_value (object patch). Keep it safe and reversible.";

    nlohmann::json tension_json = {
        {"id", tension.id},
        {"type", static_cast<int>(tension.type)},
        {"source_agent", tension.source_agent},
        {"description", tension.description},
        {"severity", tension.severity},
        {"metadata", tension.metadata}
    };

    const std::string user_prompt =
        "Generate one evolution proposal for this tension:\n" + tension_json.dump(2) +
        "\nReturn strict JSON only.";

    const auto response = llm_client->ask(system_prompt, user_prompt);
    if (!response.success || response.content.empty()) {
        return false;
    }

    const auto parsed = parse_json_object(response.content);
    if (parsed.has_value()) {
        const auto& obj = *parsed;

        if (obj.contains("description") && obj["description"].is_string()) {
            proposal->description = obj["description"].get<std::string>();
        }
        if (obj.contains("rationale") && obj["rationale"].is_string()) {
            proposal->rationale = obj["rationale"].get<std::string>();
        }

        if (obj.contains("new_value") && obj["new_value"].is_object()) {
            proposal->new_value = obj["new_value"];
        } else if (obj.contains("patch") && obj["patch"].is_object()) {
            proposal->new_value = {
                {"action", "update_config"},
                {"patch", obj["patch"]}
            };
        }
    } else {
        proposal->new_value = {
            {"action", "update_prompt"},
            {"patch", {
                {"candidate_prompt", truncate_text(response.content, 600)}
            }}
        };
    }

    if (!proposal->new_value.is_object()) {
        proposal->new_value = nlohmann::json::object();
    }
    proposal->new_value["generated_by"] = "llm";
    proposal->new_value["model"] = llm_client->config().model;
    proposal->new_value["prompt_tokens"] = response.prompt_tokens;
    proposal->new_value["completion_tokens"] = response.completion_tokens;

    return true;
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

void Evolver::set_llm_client(std::shared_ptr<llm::LLMClient> client) {
    llm_client_ = std::move(client);
}

std::vector<Tension> Evolver::monitor(const memory::OrgLog& log) {
    std::vector<Tension> tensions;

    if (!budget_.can_evolve()) {
        return tensions;
    }
    budget_.record_cycle();

    const auto& entries = log.all_entries();
    if (entries.empty()) {
        return tensions;
    }

    std::unordered_map<AgentId, std::vector<double>> agent_scores;
    std::unordered_map<AgentId, int> agent_failures;

    for (const auto& entry : entries) {
        agent_scores[entry.agent_id].push_back(std::clamp(entry.critic_score, 0.0, 1.0));
        if (entry.critic_score < 0.5) {
            agent_failures[entry.agent_id]++;
        }
    }

    for (const auto& [agent_id, scores] : agent_scores) {
        if (scores.size() < 2) {
            continue;
        }

        const double total = std::accumulate(scores.begin(), scores.end(), 0.0);
        const double avg = total / static_cast<double>(scores.size());

        const std::size_t recent_n = std::min(scores.size() / 2, static_cast<std::size_t>(5));
        if (recent_n == 0) {
            continue;
        }

        double recent_total = 0.0;
        for (std::size_t i = scores.size() - recent_n; i < scores.size(); ++i) {
            recent_total += scores[i];
        }
        const double recent_avg = recent_total / static_cast<double>(recent_n);

        const EwmaStats ewma = compute_ewma_stats(scores, config_.ewma_decay);
        const double adaptive_threshold = std::clamp(
            config_.kpi_decline_threshold + ewma.volatility * config_.volatility_sensitivity,
            0.5,
            0.98);

        if (avg > 0.0 && recent_avg < avg * adaptive_threshold) {
            Tension t;
            t.id = generate_uuid();
            t.type = TensionType::KPI_DECLINE;
            t.source_agent = agent_id;
            t.description = "KPI decline detected: recent avg " +
                            std::to_string(recent_avg) + " < threshold " +
                            std::to_string(avg * adaptive_threshold);
            t.severity = ewma.volatility > 0.2 ? "high" : "medium";
            t.metadata = {
                {"agent_id", agent_id},
                {"recent_avg", recent_avg},
                {"overall_avg", avg},
                {"ewma_score", ewma.score},
                {"ewma_volatility", ewma.volatility},
                {"adaptive_threshold", adaptive_threshold}
            };
            tensions.push_back(std::move(t));
        }
    }

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
        assign_default_proposal_fields(tension, &proposal);

        if (llm_client_ && !llm_client_->is_mock_mode()) {
            enrich_proposal_with_llm(tension, llm_client_.get(), &proposal);
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
    result.min_sample_met = result.sample_size >= std::max(1, config_.min_sample_size);

    if (control_scores.empty() || candidate_scores.empty()) {
        return result;
    }

    std::vector<double> clipped_control;
    std::vector<double> clipped_candidate;
    clipped_control.reserve(control_scores.size());
    clipped_candidate.reserve(candidate_scores.size());

    for (double value : control_scores) {
        clipped_control.push_back(std::clamp(value, 0.0, 1.0));
    }
    for (double value : candidate_scores) {
        clipped_candidate.push_back(std::clamp(value, 0.0, 1.0));
    }

    const double control_sum = std::accumulate(clipped_control.begin(), clipped_control.end(), 0.0);
    const double candidate_sum = std::accumulate(clipped_candidate.begin(), clipped_candidate.end(), 0.0);

    result.control_score = control_sum / static_cast<double>(clipped_control.size());
    result.candidate_score = candidate_sum / static_cast<double>(clipped_candidate.size());

    const double mean_delta = result.candidate_score - result.control_score;
    if (result.control_score > 1e-9) {
        result.improvement = mean_delta / result.control_score;
    } else {
        result.improvement = mean_delta;
    }

    const double control_variance = sample_variance(clipped_control);
    const double candidate_variance = sample_variance(clipped_candidate);
    const double n_control = static_cast<double>(clipped_control.size());
    const double n_candidate = static_cast<double>(clipped_candidate.size());
    const double standard_error = std::sqrt((control_variance / n_control) + (candidate_variance / n_candidate));

    if (standard_error > 1e-9) {
        const double z = mean_delta / standard_error;
        result.p_value = two_sided_p_value_from_z(z);
    } else {
        result.p_value = mean_delta > 0.0 ? 0.0 : 1.0;
    }

    result.confidence = std::clamp(1.0 - result.p_value, 0.0, 1.0);

    result.significant = result.min_sample_met &&
                         result.improvement >= config_.min_improvement &&
                         result.p_value <= config_.p_value_threshold &&
                         result.confidence >= config_.confidence_threshold;

    return result;
}

bool Evolver::apply_evolution(const EvolutionProposal& proposal,
                              const ABTestResult& test_result) const {
    if (!test_result.min_sample_met || !test_result.significant ||
        test_result.improvement < config_.min_improvement ||
        test_result.confidence < config_.confidence_threshold) {
        return false;
    }

    const bool requires_confirmation =
        proposal.type == EvolutionType::REPLACEMENT ||
        proposal.type == EvolutionType::RESTRUCTURING ||
        (proposal.new_value.is_object() && proposal.new_value.value("high_risk", false));

    nlohmann::json details = {
        {"proposal_id", proposal.id},
        {"improvement", test_result.improvement},
        {"p_value", test_result.p_value},
        {"confidence", test_result.confidence},
        {"sample_size", test_result.sample_size},
        {"requires_confirmation", requires_confirmation},
        {"high_impact", requires_confirmation}
    };

    auto permission = governance_.evaluate_action(
        "evolver", proposal.description, details);

    if (permission != governance::Permission::ALLOW) {
        return false;
    }

    if (governance_.should_rollback(
            proposal.target_agent,
            test_result.candidate_score,
            test_result.control_score)) {
        return false;
    }

    return true;
}

bool Evolver::can_evolve() const {
    return budget_.can_evolve();
}

nlohmann::json Evolver::budget_status() const {
    nlohmann::json status = budget_.status();
    status["max_proposals_per_cycle"] = config_.max_proposals_per_cycle;
    status["evolution_token_budget"] = config_.evolution_token_budget;
    status["ewma_decay"] = config_.ewma_decay;
    status["volatility_sensitivity"] = config_.volatility_sensitivity;
    status["min_sample_size"] = config_.min_sample_size;
    status["confidence_threshold"] = config_.confidence_threshold;
    return status;
}

} // namespace evoclaw::evolution
