#include "agent/critic.hpp"

#include <algorithm>
#include <string>

namespace evoclaw::agent {

double Critic::score(const TaskResult& result) const {
    double quality = result.success ? 0.6 : 0.25;
    quality += std::clamp(result.confidence, 0.0, 1.0) * 0.3;
    if (!result.output.empty()) {
        quality += 0.1;
    }
    return std::clamp(quality, 0.0, 1.0);
}

TaskResult Critic::execute(const Task& task) {
    TaskResult evaluated;
    evaluated.task_id = task.id;
    evaluated.agent_id = task.context.value("target_agent", std::string("unknown"));
    evaluated.success = task.context.value("success", true);
    evaluated.output = task.context.value("result_output", task.description);
    evaluated.confidence = task.context.value("result_confidence", 0.5);

    const double quality = score(evaluated);
    const std::string verdict = quality >= 0.7 ? "accept" : "revise";

    TaskResult result;
    result.task_id = task.id;
    result.agent_id = id();
    result.success = true;
    result.output = "Critic verdict: " + verdict;
    result.metadata = {
        {"score", quality},
        {"verdict", verdict},
        {"target_agent", evaluated.agent_id}
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
