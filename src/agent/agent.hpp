#pragma once

#include "core/types.hpp"
#include "llm/llm_client.hpp"
#include "protocol/bus.hpp"
#include "protocol/message.hpp"
#include "umi/contract.hpp"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace evoclaw::agent {

struct AgentConfig {
    AgentId id;
    std::string role;
    std::string system_prompt;
    umi::ModuleContract contract;
    double temperature = 0.3;
};

struct Task {
    struct Budget {
        int max_tokens = 4096;
        int max_tool_calls = 8;
        int max_rounds = 3;
    };

    TaskId id;
    std::string description;
    nlohmann::json context = nlohmann::json::object();
    TaskType type = TaskType::EXECUTE;
    Budget budget{};
};

struct TaskResult {
    TaskId task_id;
    AgentId agent_id;
    bool success = false;
    std::string output;
    nlohmann::json metadata = nlohmann::json::object();
    double confidence = 0.0;
};

struct Reflection {
    AgentId agent_id;
    std::string summary;
    std::vector<std::string> improvements;
    Timestamp created_at = now();
};

class Agent {
public:
    explicit Agent(AgentConfig config);
    virtual ~Agent() = default;

    [[nodiscard]] const AgentId& id() const { return config_.id; }
    [[nodiscard]] const std::string& role() const { return config_.role; }
    [[nodiscard]] const umi::ModuleContract& contract() const { return config_.contract; }

    virtual TaskResult execute(const Task& task);
    virtual Reflection reflect(const TaskResult& result);
    virtual void on_message(const protocol::Message& msg);

    void set_message_bus(std::shared_ptr<protocol::MessageBus> bus) { bus_ = std::move(bus); }
    void set_llm_client(std::shared_ptr<llm::LLMClient> client) { llm_client_ = std::move(client); }

protected:
    [[nodiscard]] llm::LLMResponse call_llm(const std::string& system_prompt,
                                            const std::string& user_message) const;
    [[nodiscard]] bool is_budget_exceeded(const Task& task, int estimated_output_tokens = 0) const;
    [[nodiscard]] TaskResult make_budget_exceeded_result(const Task& task,
                                                         int estimated_output_tokens = 0) const;

    AgentConfig config_;
    std::shared_ptr<protocol::MessageBus> bus_;
    std::shared_ptr<llm::LLMClient> llm_client_;
};

} // namespace evoclaw::agent
