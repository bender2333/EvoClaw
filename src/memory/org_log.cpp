#include "memory/org_log.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <sstream>
#include <utility>

namespace evoclaw::memory {
namespace {

using Milliseconds = std::chrono::milliseconds;

std::int64_t to_epoch_millis(const Timestamp ts) {
    return std::chrono::duration_cast<Milliseconds>(ts.time_since_epoch()).count();
}

Timestamp from_epoch_millis(const std::int64_t millis) {
    return Timestamp{Milliseconds(millis)};
}

nlohmann::json to_json_entry(const OrgLogEntry& entry) {
    return {
        {"entry_id", entry.entry_id},
        {"timestamp_ms", to_epoch_millis(entry.timestamp)},
        {"task_id", entry.task_id},
        {"agent_id", entry.agent_id},
        {"module_version", entry.module_version},
        {"duration_ms", entry.duration_ms},
        {"critic_score", entry.critic_score},
        {"user_feedback_positive", entry.user_feedback_positive},
        {"metadata", entry.metadata}
    };
}

std::optional<OrgLogEntry> from_json_entry(const nlohmann::json& obj) {
    if (!obj.is_object()) {
        return std::nullopt;
    }

    OrgLogEntry entry;
    entry.entry_id = obj.value("entry_id", "");
    entry.timestamp = from_epoch_millis(obj.value("timestamp_ms", static_cast<std::int64_t>(0)));
    entry.task_id = obj.value("task_id", "");
    entry.agent_id = obj.value("agent_id", "");
    entry.module_version = obj.value("module_version", "");
    entry.duration_ms = obj.value("duration_ms", 0.0);
    entry.critic_score = obj.value("critic_score", 0.0);
    entry.user_feedback_positive = obj.value("user_feedback_positive", false);
    entry.metadata = obj.value("metadata", nlohmann::json::object());
    return entry;
}

std::optional<OrgLogEntry> parse_line(const std::string& line) {
    if (line.empty()) {
        return std::nullopt;
    }
    try {
        return from_json_entry(nlohmann::json::parse(line));
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

OrgLog::OrgLog(const std::filesystem::path& log_dir)
    : log_dir_(log_dir),
      log_path_(log_dir_ / "org_log.jsonl") {
    std::error_code ec;
    std::filesystem::create_directories(log_dir_, ec);
    log_file_.open(log_path_, std::ios::app);
}

void OrgLog::append(const OrgLogEntry& entry) {
    std::scoped_lock lock(mutex_);
    if (!log_file_.is_open()) {
        log_file_.open(log_path_, std::ios::app);
    }
    if (!log_file_.is_open()) {
        return;
    }

    OrgLogEntry normalized = entry;
    if (normalized.entry_id.empty()) {
        normalized.entry_id = generate_uuid();
    }
    if (normalized.timestamp.time_since_epoch() == Timestamp::duration::zero()) {
        normalized.timestamp = now();
    }

    log_file_ << to_json_entry(normalized).dump() << '\n';
    log_file_.flush();
}

std::vector<OrgLogEntry> OrgLog::query_by_agent(const AgentId& agent, const int limit) const {
    if (agent.empty() || limit <= 0) {
        return {};
    }

    std::scoped_lock lock(mutex_);
    std::ifstream in(log_path_);
    if (!in.is_open()) {
        return {};
    }

    std::vector<OrgLogEntry> results;
    results.reserve(static_cast<std::size_t>(std::min(limit, 256)));

    std::string line;
    while (std::getline(in, line)) {
        const auto parsed = parse_line(line);
        if (!parsed.has_value()) {
            continue;
        }
        if (parsed->agent_id != agent) {
            continue;
        }
        results.push_back(*parsed);
        if (static_cast<int>(results.size()) > limit) {
            results.erase(results.begin());
        }
    }

    return results;
}

std::vector<OrgLogEntry> OrgLog::query_by_time_range(Timestamp start, Timestamp end) const {
    if (start > end) {
        std::swap(start, end);
    }

    std::scoped_lock lock(mutex_);
    std::ifstream in(log_path_);
    if (!in.is_open()) {
        return {};
    }

    std::vector<OrgLogEntry> results;
    std::string line;
    while (std::getline(in, line)) {
        const auto parsed = parse_line(line);
        if (!parsed.has_value()) {
            continue;
        }
        if (parsed->timestamp >= start && parsed->timestamp <= end) {
            results.push_back(*parsed);
        }
    }
    return results;
}

double OrgLog::average_score(const AgentId& agent) const {
    if (agent.empty()) {
        return 0.0;
    }

    std::scoped_lock lock(mutex_);
    std::ifstream in(log_path_);
    if (!in.is_open()) {
        return 0.0;
    }

    double score_sum = 0.0;
    std::size_t count = 0;

    std::string line;
    while (std::getline(in, line)) {
        const auto parsed = parse_line(line);
        if (!parsed.has_value() || parsed->agent_id != agent) {
            continue;
        }
        score_sum += parsed->critic_score;
        ++count;
    }

    if (count == 0U) {
        return 0.0;
    }
    return score_sum / static_cast<double>(count);
}

std::vector<OrgLogEntry> OrgLog::all_entries() const {
    std::scoped_lock lock(mutex_);
    std::ifstream in(log_path_);
    if (!in.is_open()) {
        return {};
    }

    std::vector<OrgLogEntry> results;
    std::string line;
    while (std::getline(in, line)) {
        const auto parsed = parse_line(line);
        if (parsed.has_value()) {
            results.push_back(*parsed);
        }
    }
    return results;
}

} // namespace evoclaw::memory
