#include "agent/agent.hpp"

#include <utility>

namespace evoclaw::agent {

Agent::Agent(AgentConfig config)
    : config_(std::move(config)),
      bus_(std::make_shared<protocol::MessageBus>()) {}

TaskResult Agent::execute(const Task& task) {
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

} // namespace evoclaw::agent
