#include "agent/planner.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace evoclaw::agent {

TaskResult Planner::execute(const Task& task) {
    std::vector<std::string> steps;
    steps.push_back("Understand objective: " + (task.description.empty() ? std::string("unspecified") : task.description));

    if (task.context.contains("constraints") && task.context["constraints"].is_array()) {
        for (const auto& constraint : task.context["constraints"]) {
            if (constraint.is_string()) {
                steps.push_back("Respect constraint: " + constraint.get<std::string>());
            }
        }
    }

    steps.push_back("Break work into actionable subtasks");
    steps.push_back("Define validation and success criteria");

    std::ostringstream output;
    output << "Generated plan:\n";
    for (std::size_t i = 0; i < steps.size(); ++i) {
        output << (i + 1) << ". " << steps[i] << '\n';
    }

    TaskResult result;
    result.task_id = task.id;
    result.agent_id = id();
    result.success = true;
    result.output = output.str();
    result.metadata = {
        {"steps", steps},
        {"role", role()}
    };
    result.confidence = 0.78;
    return result;
}

Reflection Planner::reflect(const TaskResult& result) {
    Reflection reflection;
    reflection.agent_id = id();
    reflection.summary = result.success ? "Plan quality is acceptable for execution." :
                                          "Plan generation failed and needs refinement.";
    reflection.improvements = {
        "Include explicit risk mitigation for each step",
        "Attach concrete acceptance checks to every milestone"
    };
    reflection.created_at = now();
    return reflection;
}

} // namespace evoclaw::agent
