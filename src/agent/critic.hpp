#pragma once

#include "agent/agent.hpp"

namespace evoclaw::agent {

class Critic : public Agent {
public:
    using Agent::Agent;

    TaskResult execute(const Task& task) override;
    Reflection reflect(const TaskResult& result) override;

    [[nodiscard]] double score(const TaskResult& result) const;
};

} // namespace evoclaw::agent
