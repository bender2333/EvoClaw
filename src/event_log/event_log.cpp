#include "event_log/event_log.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace evoclaw::event_log {
namespace {

using Milliseconds = std::chrono::milliseconds;

std::int64_t to_epoch_millis(const Timestamp ts) {
    return std::chrono::duration_cast<Milliseconds>(ts.time_since_epoch()).count();
}

Timestamp from_epoch_millis(const std::int64_t millis) {
    return Timestamp{Milliseconds(millis)};
}

nlohmann::json to_json_event(const Event& event) {
    return {
        {"id", event.id},
        {"timestamp_ms", to_epoch_millis(event.timestamp)},
        {"type", event.type},
        {"actor", event.actor},
        {"target", event.target},
        {"action", event.action},
        {"details", event.details},
        {"prev_hash", event.prev_hash},
        {"hash", event.hash}
    };
}

std::optional<Event> from_json_event(const nlohmann::json& obj) {
    if (!obj.is_object()) {
        return std::nullopt;
    }

    Event event;
    event.id = obj.value("id", "");
    event.timestamp = from_epoch_millis(obj.value("timestamp_ms", static_cast<std::int64_t>(0)));
    event.type = obj.value("type", EventType::TASK_COMPLETE);
    event.actor = obj.value("actor", "");
    event.target = obj.value("target", "");
    event.action = obj.value("action", "");
    event.details = obj.value("details", nlohmann::json::object());
    event.prev_hash = obj.value("prev_hash", "");
    event.hash = obj.value("hash", "");
    return event;
}

std::optional<Event> parse_line(const std::string& line) {
    if (line.empty()) {
        return std::nullopt;
    }
    try {
        return from_json_event(nlohmann::json::parse(line));
    } catch (...) {
        return std::nullopt;
    }
}

bool match_filter(const Event& event, const std::string& key, const nlohmann::json& value) {
    if (key == "id") {
        return value == event.id;
    }
    if (key == "type") {
        if (value.is_string()) {
            return value.get<std::string>() == nlohmann::json(event.type).get<std::string>();
        }
        if (value.is_number_integer()) {
            return value.get<int>() == static_cast<int>(event.type);
        }
        return false;
    }
    if (key == "actor") {
        return value == event.actor;
    }
    if (key == "target") {
        return value == event.target;
    }
    if (key == "action") {
        return value == event.action;
    }
    if (key == "hash") {
        return value == event.hash;
    }
    if (key == "prev_hash") {
        return value == event.prev_hash;
    }
    if (key == "from_ms" && value.is_number_integer()) {
        return to_epoch_millis(event.timestamp) >= value.get<std::int64_t>();
    }
    if (key == "to_ms" && value.is_number_integer()) {
        return to_epoch_millis(event.timestamp) <= value.get<std::int64_t>();
    }
    if (key == "details") {
        if (value.is_object() && event.details.is_object()) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                if (!event.details.contains(it.key()) || event.details[it.key()] != it.value()) {
                    return false;
                }
            }
            return true;
        }
        return value == event.details;
    }
    if (key.rfind("details.", 0) == 0) {
        const std::string detail_key = key.substr(std::string("details.").size());
        if (!event.details.is_object() || !event.details.contains(detail_key)) {
            return false;
        }
        return event.details[detail_key] == value;
    }

    return false;
}

} // namespace

EventLog::EventLog(const std::filesystem::path& log_path)
    : log_path_(log_path) {
    std::error_code ec;
    if (!log_path_.parent_path().empty()) {
        std::filesystem::create_directories(log_path_.parent_path(), ec);
    }

    log_file_.open(log_path_, std::ios::app);
    cached_last_event_ = last_event();
}

void EventLog::append(Event event) {
    std::scoped_lock lock(mutex_);
    if (!log_file_.is_open()) {
        log_file_.open(log_path_, std::ios::app);
    }
    if (!log_file_.is_open()) {
        return;
    }

    if (event.id.empty()) {
        event.id = generate_uuid();
    }
    if (event.timestamp.time_since_epoch() == Timestamp::duration::zero()) {
        event.timestamp = now();
    }

    event.prev_hash = cached_last_event_.has_value() ? cached_last_event_->hash : "";
    event.hash = calculate_hash(event);

    log_file_ << to_json_event(event).dump() << '\n';
    log_file_.flush();
    cached_last_event_ = event;
}

std::vector<Event> EventLog::query(const std::unordered_map<std::string, nlohmann::json>& filters) const {
    std::scoped_lock lock(mutex_);
    std::ifstream in(log_path_);
    if (!in.is_open()) {
        return {};
    }

    std::vector<Event> results;
    std::string line;
    while (std::getline(in, line)) {
        const auto parsed = parse_line(line);
        if (!parsed.has_value()) {
            continue;
        }

        bool matches = true;
        for (const auto& [key, value] : filters) {
            if (!match_filter(*parsed, key, value)) {
                matches = false;
                break;
            }
        }
        if (matches) {
            results.push_back(*parsed);
        }
    }

    return results;
}

bool EventLog::verify_integrity() const {
    std::scoped_lock lock(mutex_);
    std::ifstream in(log_path_);
    if (!in.is_open()) {
        return true;
    }

    std::string expected_prev_hash;
    std::string line;
    while (std::getline(in, line)) {
        const auto parsed = parse_line(line);
        if (!parsed.has_value()) {
            return false;
        }

        if (parsed->prev_hash != expected_prev_hash) {
            return false;
        }

        if (calculate_hash(*parsed) != parsed->hash) {
            return false;
        }

        expected_prev_hash = parsed->hash;
    }
    return true;
}

std::optional<Event> EventLog::last_event() const {
    std::ifstream in(log_path_);
    if (!in.is_open()) {
        return std::nullopt;
    }

    std::optional<Event> last;
    std::string line;
    while (std::getline(in, line)) {
        const auto parsed = parse_line(line);
        if (parsed.has_value()) {
            last = parsed;
        }
    }

    return last;
}

std::string EventLog::calculate_hash(const Event& event) {
    nlohmann::json to_hash = {
        {"id", event.id},
        {"timestamp_ms", to_epoch_millis(event.timestamp)},
        {"type", event.type},
        {"actor", event.actor},
        {"target", event.target},
        {"action", event.action},
        {"details", event.details},
        {"prev_hash", event.prev_hash}
    };

    const std::string payload = to_hash.dump();
    const std::size_t h1 = std::hash<std::string>{}(payload);
    const std::size_t h2 = std::hash<std::string>{}(payload + "|evoclaw");

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(static_cast<int>(sizeof(std::size_t) * 2)) << h1
        << std::setw(static_cast<int>(sizeof(std::size_t) * 2)) << h2;
    return oss.str();
}

} // namespace evoclaw::event_log
