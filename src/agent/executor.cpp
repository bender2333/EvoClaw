#include "agent/executor.hpp"

#include <string>

namespace evoclaw::agent {

TaskResult Executor::execute(const Task& task) {
    std::string plan = task.description;
    if (task.context.contains("plan") && task.context["plan"].is_string()) {
        plan = task.context["plan"].get<std::string>();
    }
    if (plan.empty()) {
        plan = "no plan provided";
    }

    TaskResult result;
    result.task_id = task.id;
    result.agent_id = id();
    result.success = true;
    result.output = "Executed plan: " + plan;
    result.metadata = {
        {"plan_used", plan},
        {"role", role()},
        {"status", "mock_executed"}
    };
    result.confidence = 0.82;
    return result;
}

Reflection Executor::reflect(const TaskResult& result) {
    Reflection reflection;
    reflection.agent_id = id();
    reflection.summary = result.success ? "Execution completed with mock output." :
                                          "Execution did not complete successfully.";
    reflection.improvements = {
        "Capture tool-level trace for each execution step",
        "Add retry strategy for transient execution failures"
    };
    reflection.created_at = now();
    return reflection;
}

} // namespace evoclaw::agent
