#pragma once

#include "core/types.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace evoclaw::memory {

struct OrgLogEntry {
    std::string entry_id;
    Timestamp timestamp = now();
    TaskId task_id;
    AgentId agent_id;
    std::string module_version;
    double duration_ms = 0.0;
    double critic_score = 0.0;
    bool user_feedback_positive = false;
    nlohmann::json metadata = nlohmann::json::object();
};

class OrgLog {
public:
    struct TimeRangeStats {
        int total_tasks = 0;
        int successful_tasks = 0;
        double avg_duration_ms = 0.0;
        double avg_critic_score = 0.0;
        std::unordered_map<AgentId, int> tasks_by_agent;
    };

    explicit OrgLog(const std::filesystem::path& log_dir);

    void append(const OrgLogEntry& entry);

    [[nodiscard]] std::vector<OrgLogEntry> query_by_agent(const AgentId& agent, int limit = 100) const;
    [[nodiscard]] std::vector<OrgLogEntry> query_by_time_range(Timestamp start, Timestamp end) const;
    [[nodiscard]] std::vector<OrgLogEntry> query_by_agent_and_time(const AgentId& agent_id,
                                                                    Timestamp start,
                                                                    Timestamp end) const;
    [[nodiscard]] TimeRangeStats get_stats_for_range(Timestamp start, Timestamp end) const;
    [[nodiscard]] double average_score(const AgentId& agent) const;
    [[nodiscard]] std::vector<OrgLogEntry> all_entries() const;

private:
    std::filesystem::path log_dir_;
    std::filesystem::path log_path_;
    std::ofstream log_file_;
    mutable std::mutex mutex_;
};

} // namespace evoclaw::memory
