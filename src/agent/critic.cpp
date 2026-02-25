#include "agent/critic.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace evoclaw::agent {
namespace {

std::optional<nlohmann::json> parse_json_payload(std::string text) {
    try {
        return nlohmann::json::parse(text);
    } catch (...) {
        // Fall through to extraction.
    }

    const std::size_t begin = text.find('{');
    const std::size_t end = text.rfind('}');
    if (begin == std::string::npos || end == std::string::npos || end < begin) {
        return std::nullopt;
    }

    text = text.substr(begin, end - begin + 1U);
    try {
        return nlohmann::json::parse(text);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

double Critic::score(const TaskResult& result) const {
    double quality = result.success ? 0.6 : 0.25;
    quality += std::clamp(result.confidence, 0.0, 1.0) * 0.3;
    if (!result.output.empty()) {
        quality += 0.1;
    }
    return std::clamp(quality, 0.0, 1.0);
}

TaskResult Critic::execute(const Task& task) {
    if (is_budget_exceeded(task)) {
        return make_budget_exceeded_result(task);
    }

    if (llm_client_) {
        const auto response = call_llm(
            "You are a quality critic. Evaluate the given work and provide:\n"
            "1. A score from 0.0 to 1.0\n"
            "2. A verdict: accept or reject\n"
            "3. Brief feedback\n"
            "Output as JSON: {\"score\": 0.85, \"verdict\": \"accept\", \"feedback\": \"...\"}",
            "Work to evaluate: " + task.description + "\nContext: " + task.context.dump());

        if (response.success) {
            const auto parsed = parse_json_payload(response.content);
            if (parsed.has_value() && parsed->is_object()) {
                const double quality = std::clamp(parsed->value("score", 0.0), 0.0, 1.0);
                const std::string verdict = parsed->value("verdict", std::string("reject"));
                const std::string feedback = parsed->value("feedback", std::string(""));

                TaskResult result;
                result.task_id = task.id;
                result.agent_id = id();
                result.success = true;
                result.output = "Critic verdict: " + verdict + (feedback.empty() ? "" : " | " + feedback);
                result.metadata = {
                    {"score", quality},
                    {"verdict", verdict},
                    {"feedback", feedback},
                    {"source", "llm"},
                    {"model", llm_client_->config().model},
                    {"prompt_tokens", response.prompt_tokens},
                    {"completion_tokens", response.completion_tokens}
                };
                result.confidence = quality;
                return result;
            }
        }
    }

    TaskResult evaluated;
    evaluated.task_id = task.id;
    evaluated.agent_id = task.context.value("target_agent", std::string("unknown"));
    evaluated.success = task.context.value("success", true);
    evaluated.output = task.context.value("result_output", task.description);
    evaluated.confidence = task.context.value("result_confidence", 0.5);

    const double quality = score(evaluated);
    const std::string verdict = quality >= 0.7 ? "accept" : "reject";

    TaskResult result;
    result.task_id = task.id;
    result.agent_id = id();
    result.success = true;
    result.output = "Critic verdict: " + verdict;
    result.metadata = {
        {"score", quality},
        {"verdict", verdict},
        {"target_agent", evaluated.agent_id},
        {"source", "mock"}
    };
    result.confidence = quality;
    return result;
}

Reflection Critic::reflect(const TaskResult& result) {
    const double quality = result.metadata.value("score", result.confidence);

    Reflection reflection;
    reflection.agent_id = id();
    reflection.summary = quality >= 0.7 ? "Result quality is strong." : "Result quality needs improvement.";

    if (quality < 0.5) {
        reflection.improvements = {
            "Strengthen the execution plan with clearer milestones",
            "Increase validation depth before final submission"
        };
    } else if (quality < 0.7) {
        reflection.improvements = {
            "Improve result clarity with concrete evidence",
            "Tighten consistency checks before handoff"
        };
    } else {
        reflection.improvements = {
            "Preserve current approach and monitor regressions"
        };
    }

    reflection.created_at = now();
    return reflection;
}

} // namespace evoclaw::agent
