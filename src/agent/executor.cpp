#include "agent/executor.hpp"

#include <string>

namespace evoclaw::agent {

TaskResult Executor::execute(const Task& task) {
    reset_token_consumption();
    if (is_budget_exceeded(task)) {
        return make_budget_exceeded_result(task);
    }

    if (llm_client_) {
        const auto response = call_llm(
            "You are a task executor. Given a task description, produce the output. "
            "Be thorough and accurate.",
            "Task: " + task.description + "\nContext: " + task.context.dump());

        if (response.success) {
            TaskResult result;
            result.task_id = task.id;
            result.agent_id = id();
            result.success = true;
            result.output = response.content;
            result.metadata = {
                {"role", role()},
                {"source", "llm"},
                {"model", llm_client_->config().model},
                {"prompt_tokens", response.prompt_tokens},
                {"completion_tokens", response.completion_tokens}
            };
            result.confidence = 0.85;
            return result;
        }
    }

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
        {"status", "mock_executed"},
        {"source", "mock"}
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
