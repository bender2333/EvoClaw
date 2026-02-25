#pragma once

#include "agent/agent.hpp"

namespace evoclaw::agent {

class Planner : public Agent {
public:
    using Agent::Agent;

    TaskResult execute(const Task& task) override;
    Reflection reflect(const TaskResult& result) override;
};

} // namespace evoclaw::agent
