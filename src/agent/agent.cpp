#include "agent/agent.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace evoclaw::agent {

Agent::Agent(AgentConfig config)
    : config_(std::move(config)),
      bus_(std::make_shared<protocol::MessageBus>()) {}

TaskResult Agent::execute(const Task& task) {
    reset_token_consumption();
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



nlohmann::json Agent::runtime_config() const {
    return {
        {"id", config_.id},
        {"role", config_.role},
        {"system_prompt", config_.system_prompt},
        {"temperature", config_.temperature},
        {"contract", {
            {"module_id", config_.contract.module_id},
            {"version", config_.contract.version},
            {"success_rate_threshold", config_.contract.capability.success_rate_threshold},
            {"estimated_cost_token", config_.contract.capability.estimated_cost_token},
            {"intent_tags", config_.contract.capability.intent_tags},
            {"required_tools", config_.contract.capability.required_tools}
        }}
    };
}

bool Agent::apply_runtime_patch(const nlohmann::json& patch, std::string* reason) {
    if (!patch.is_object()) {
        if (reason) {
            *reason = "patch must be a JSON object";
        }
        return false;
    }

    const AgentConfig backup = config_;
    const auto fail = [&](const std::string& message) {
        config_ = backup;
        if (reason) {
            *reason = message;
        }
        return false;
    };

    bool changed = false;

    if (patch.contains("system_prompt") && patch["system_prompt"].is_string()) {
        config_.system_prompt = patch["system_prompt"].get<std::string>();
        changed = true;
    }

    if (patch.contains("system_prompt_suffix") && patch["system_prompt_suffix"].is_string()) {
        const auto suffix = patch["system_prompt_suffix"].get<std::string>();
        if (!suffix.empty()) {
            if (!config_.system_prompt.empty() && config_.system_prompt.back() != '\n') {
                config_.system_prompt.push_back('\n');
            }
            config_.system_prompt += suffix;
            changed = true;
        }
    }

    if (patch.contains("temperature") && patch["temperature"].is_number()) {
        const double temperature = patch["temperature"].get<double>();
        if (temperature < 0.0 || temperature > 2.0) {
            return fail("temperature must be in [0, 2]");
        }
        config_.temperature = temperature;
        changed = true;
    }

    if (patch.contains("success_rate_threshold") && patch["success_rate_threshold"].is_number()) {
        const double threshold = patch["success_rate_threshold"].get<double>();
        if (threshold < 0.0 || threshold > 1.0) {
            return fail("success_rate_threshold must be in [0, 1]");
        }
        config_.contract.capability.success_rate_threshold = threshold;
        changed = true;
    }

    if (patch.contains("estimated_cost_token") && patch["estimated_cost_token"].is_number()) {
        const double cost = patch["estimated_cost_token"].get<double>();
        if (cost <= 0.0) {
            return fail("estimated_cost_token must be positive");
        }
        config_.contract.capability.estimated_cost_token = cost;
        changed = true;
    }

    if (patch.contains("intent_tags") && patch["intent_tags"].is_array()) {
        std::vector<std::string> tags;
        for (const auto& item : patch["intent_tags"]) {
            if (!item.is_string()) {
                return fail("intent_tags must be an array of strings");
            }
            tags.push_back(item.get<std::string>());
        }
        config_.contract.capability.intent_tags = std::move(tags);
        changed = true;
    }

    if (patch.contains("required_tools") && patch["required_tools"].is_array()) {
        std::vector<std::string> tools;
        for (const auto& item : patch["required_tools"]) {
            if (!item.is_string()) {
                return fail("required_tools must be an array of strings");
            }
            tools.push_back(item.get<std::string>());
        }
        config_.contract.capability.required_tools = std::move(tools);
        changed = true;
    }

    if (patch.contains("module_version") && patch["module_version"].is_string()) {
        config_.contract.version = patch["module_version"].get<std::string>();
        changed = true;
    }

    if (!changed) {
        return fail("patch had no supported fields");
    }

    return true;
}

bool Agent::restore_runtime_config(const nlohmann::json& snapshot, std::string* reason) {
    if (!snapshot.is_object()) {
        if (reason) {
            *reason = "snapshot must be a JSON object";
        }
        return false;
    }

    nlohmann::json patch = nlohmann::json::object();
    if (snapshot.contains("system_prompt")) {
        patch["system_prompt"] = snapshot["system_prompt"];
    }
    if (snapshot.contains("temperature")) {
        patch["temperature"] = snapshot["temperature"];
    }

    if (snapshot.contains("contract") && snapshot["contract"].is_object()) {
        const auto& contract = snapshot["contract"];
        if (contract.contains("version")) {
            patch["module_version"] = contract["version"];
        }
        if (contract.contains("success_rate_threshold")) {
            patch["success_rate_threshold"] = contract["success_rate_threshold"];
        }
        if (contract.contains("estimated_cost_token")) {
            patch["estimated_cost_token"] = contract["estimated_cost_token"];
        }
        if (contract.contains("intent_tags")) {
            patch["intent_tags"] = contract["intent_tags"];
        }
        if (contract.contains("required_tools")) {
            patch["required_tools"] = contract["required_tools"];
        }
    }

    return apply_runtime_patch(patch, reason);
}
void Agent::reset_token_consumption() {
    tokens_consumed_ = 0;
    prompt_tokens_consumed_ = 0;
    completion_tokens_consumed_ = 0;
}

llm::LLMResponse Agent::call_llm(const std::string& system_prompt, const std::string& user_message) {
    if (!llm_client_) {
        return {
            .success = true,
            .content = "Mock LLM response: no client injected.",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = ""
        };
    }

    llm::LLMResponse response = llm_client_->ask(system_prompt, user_message);
    const int prompt_tokens = std::max(response.prompt_tokens, 0);
    const int completion_tokens = std::max(response.completion_tokens, 0);
    prompt_tokens_consumed_ += prompt_tokens;
    completion_tokens_consumed_ += completion_tokens;
    tokens_consumed_ += prompt_tokens + completion_tokens;
    return response;
}

bool Agent::is_budget_exceeded(const Task& task, const int estimated_output_tokens) const {
    if (task.budget.max_tokens <= 0) {
        return false;
    }

    const int consumed_tokens = std::max(tokens_consumed_, 0);
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
        {"consumed_tokens", std::max(tokens_consumed_, 0)},
        {"estimated_output_tokens", std::max(estimated_output_tokens, 0)}
    };
    result.confidence = 0.0;
    return result;
}

} // namespace evoclaw::agent
