#include "agent/agent.hpp"

#include <algorithm>
#include <utility>

namespace evoclaw::agent {

Agent::Agent(AgentConfig config)
    : config_(std::move(config)),
      bus_(std::make_shared<protocol::MessageBus>()) {}

TaskResult Agent::execute(const Task& task) {
    if (is_budget_exceeded(task)) {
        return make_budget_exceeded_result(task);
    }

    TaskResult result;
    result.task_id = task.id;
    result.agent_id = id();
    result.success = false;
    result.output = "execute() is not implemented for agent role: " + role();
    result.metadata = {
        {"role", role()},
        {"status", "not_implemented"}
    };
    result.confidence = 0.0;
    return result;
}

Reflection Agent::reflect(const TaskResult& result) {
    Reflection reflection;
    reflection.agent_id = id();
    reflection.summary = "No reflection strategy implemented for role: " + role();
    reflection.improvements = {
        "Override reflect() for role-specific reflection",
        "Capture result metadata for adaptive behavior"
    };
    reflection.created_at = now();

    (void)result;
    return reflection;
}

void Agent::on_message(const protocol::Message& msg) {
    (void)msg;
}

llm::LLMResponse Agent::call_llm(const std::string& system_prompt, const std::string& user_message) const {
    if (!llm_client_) {
        return {
            .success = true,
            .content = "Mock LLM response: no client injected.",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = ""
        };
    }
    return llm_client_->ask(system_prompt, user_message);
}

bool Agent::is_budget_exceeded(const Task& task, const int estimated_output_tokens) const {
    if (task.budget.max_tokens <= 0) {
        return false;
    }

    int consumed_tokens = 0;
    if (task.context.contains("used_tokens") && task.context["used_tokens"].is_number_integer()) {
        consumed_tokens = task.context["used_tokens"].get<int>();
    } else if (task.context.contains("tokens_used") && task.context["tokens_used"].is_number_integer()) {
        consumed_tokens = task.context["tokens_used"].get<int>();
    }

    consumed_tokens = std::max(consumed_tokens, 0);
    return consumed_tokens + std::max(estimated_output_tokens, 0) > task.budget.max_tokens;
}

TaskResult Agent::make_budget_exceeded_result(const Task& task, const int estimated_output_tokens) const {
    TaskResult result;
    result.task_id = task.id;
    result.agent_id = id();
    result.success = false;
    result.output = "Budget exceeded";
    result.metadata = {
        {"reason", "budget_exceeded"},
        {"max_tokens", task.budget.max_tokens},
        {"estimated_output_tokens", std::max(estimated_output_tokens, 0)}
    };
    result.confidence = 0.0;
    return result;
}

} // namespace evoclaw::agent
