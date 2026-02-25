#pragma once

#include "core/types.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace evoclaw::event_log {

enum class EventType {
    EVOLUTION,
    CIRCUIT_BREAK,
    ROLLBACK,
    AGENT_SPAWN,
    AGENT_RETIRE,
    GUARDRAIL_BLOCK,
    CONFIG_CHANGE,
    TASK_COMPLETE,
    TASK_FAILED
};

NLOHMANN_JSON_SERIALIZE_ENUM(EventType, {
    {EventType::EVOLUTION, "evolution"},
    {EventType::CIRCUIT_BREAK, "circuit_break"},
    {EventType::ROLLBACK, "rollback"},
    {EventType::AGENT_SPAWN, "agent_spawn"},
    {EventType::AGENT_RETIRE, "agent_retire"},
    {EventType::GUARDRAIL_BLOCK, "guardrail_block"},
    {EventType::CONFIG_CHANGE, "config_change"},
    {EventType::TASK_COMPLETE, "task_complete"},
    {EventType::TASK_FAILED, "task_failed"}
})

struct Event {
    std::string id;
    Timestamp timestamp = now();
    EventType type = EventType::TASK_COMPLETE;
    std::string actor;
    std::string target;
    std::string action;
    nlohmann::json details = nlohmann::json::object();
    std::string prev_hash;
    std::string hash;
};

class EventLog {
public:
    explicit EventLog(const std::filesystem::path& log_path);

    void append(Event event);
    [[nodiscard]] std::vector<Event> query(const std::unordered_map<std::string, nlohmann::json>& filters) const;
    [[nodiscard]] bool verify_integrity() const;

private:
    [[nodiscard]] std::optional<Event> last_event() const;
    [[nodiscard]] static std::string calculate_hash(const Event& event);

    std::filesystem::path log_path_;
    std::ofstream log_file_;
    mutable std::mutex mutex_;
    mutable std::optional<Event> cached_last_event_;
};

} // namespace evoclaw::event_log
