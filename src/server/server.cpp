#include "server/server.hpp"

#include "core/types.hpp"
#include "server/dashboard.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace evoclaw::server {
namespace {

using json = nlohmann::json;

void set_json_response(httplib::Response& res, const json& payload, const int status = 200) {
    res.status = status;
    res.set_content(payload.dump(), "application/json");
}

json build_error(const std::string& message) {
    return {
        {"ok", false},
        {"error", message}
    };
}

std::optional<std::uint64_t> parse_uint64_param(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    std::uint64_t parsed = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc() || ptr != end) {
        return std::nullopt;
    }

    return parsed;
}

std::optional<std::size_t> parse_non_negative_size_t(const json& value) {
    std::uint64_t parsed = 0;
    if (value.is_number_unsigned()) {
        parsed = value.get<std::uint64_t>();
    } else if (value.is_number_integer()) {
        const auto signed_value = value.get<std::int64_t>();
        if (signed_value < 0) {
            return std::nullopt;
        }
        parsed = static_cast<std::uint64_t>(signed_value);
    } else {
        return std::nullopt;
    }

    if (parsed > std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(parsed);
}

int runtime_config_query_status(const json& payload) {
    if (!payload.contains("error") || !payload["error"].is_string()) {
        return 200;
    }

    const auto error = payload["error"].get<std::string>();
    if (error == "agent_not_found" || error == "version_not_found") {
        return 404;
    }
    if (error == "invalid_version_range") {
        return 400;
    }
    return 400;
}

std::string event_message(const json& event) {
    if (event.contains("message") && event["message"].is_string()) {
        return event["message"].get<std::string>();
    }

    if (event.contains("description") && event["description"].is_string()) {
        return event["description"].get<std::string>();
    }

    if (event.contains("task_id")) {
        return "Task " + event.value("task_id", std::string("unknown"));
    }

    return event.dump();
}

std::optional<Timestamp> parse_iso8601_timestamp(std::string value) {
    if (value.empty()) {
        return std::nullopt;
    }

    int offset_seconds = 0;
    bool has_utc_offset = false;

    if (!value.empty() && (value.back() == 'Z' || value.back() == 'z')) {
        has_utc_offset = true;
        value.pop_back();
    } else {
        const std::size_t tz_pos = value.find_last_of("+-");
        if (tz_pos != std::string::npos && tz_pos > 10) {
            const std::string offset = value.substr(tz_pos);
            const int sign = offset[0] == '-' ? -1 : 1;
            int hours = 0;
            int minutes = 0;

            bool parsed = false;
            if (offset.size() == 6 && offset[3] == ':') {
                parsed = std::sscanf(offset.c_str() + 1, "%2d:%2d", &hours, &minutes) == 2;
            } else if (offset.size() == 5) {
                parsed = std::sscanf(offset.c_str() + 1, "%2d%2d", &hours, &minutes) == 2;
            }

            if (parsed) {
                has_utc_offset = true;
                offset_seconds = sign * ((hours * 3600) + (minutes * 60));
                value = value.substr(0, tz_pos);
            }
        }
    }

    if (value.size() > 19 && value[19] == '.') {
        value = value.substr(0, 19);
    }

    std::tm tm{};
    std::istringstream stream(value);
    stream >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (stream.fail()) {
        return std::nullopt;
    }

    std::time_t time_value = 0;
    if (has_utc_offset) {
#if defined(_WIN32)
        time_value = _mkgmtime(&tm);
#else
        time_value = timegm(&tm);
#endif
        time_value -= offset_seconds;
    } else {
        time_value = std::mktime(&tm);
    }

    if (time_value < 0) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(time_value);
}

json org_log_entry_to_json(const memory::OrgLogEntry& entry) {
    const auto timestamp_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(entry.timestamp.time_since_epoch()).count();

    return {
        {"entry_id", entry.entry_id},
        {"timestamp", timestamp_to_string(entry.timestamp)},
        {"timestamp_ms", timestamp_ms},
        {"task_id", entry.task_id},
        {"agent_id", entry.agent_id},
        {"module_version", entry.module_version},
        {"duration_ms", entry.duration_ms},
        {"critic_score", entry.critic_score},
        {"user_feedback_positive", entry.user_feedback_positive},
        {"metadata", entry.metadata}
    };
}

} // namespace

EvoClawServer::EvoClawServer(facade::EvoClawFacade& facade, const int port)
    : facade_(facade), port_(port) {
    setup_routes();

    facade_.set_event_callback([this](const nlohmann::json& event) {
        on_facade_event(event);
    });

    on_facade_event({
        {"event", "server_started"},
        {"message", "Web dashboard server is ready"},
        {"port", port_},
        {"timestamp", timestamp_to_string(now())}
    });
}

EvoClawServer::~EvoClawServer() {
    stop();
    facade_.set_event_callback(nullptr);
}

void EvoClawServer::start() {
    if (running_.exchange(true)) {
        return;
    }

    stop_requested_.store(false);
    const bool ok = server_.listen("0.0.0.0", port_);
    running_.store(false);

    if (!ok && !stop_requested_.load()) {
        throw std::runtime_error("Failed to start EvoClaw server on port " + std::to_string(port_));
    }
}

void EvoClawServer::start_async() {
    if (server_thread_.joinable()) {
        return;
    }

    server_thread_ = std::thread([this]() {
        try {
            start();
        } catch (const std::exception& ex) {
            std::cerr << "EvoClawServer background start failed: " << ex.what() << '\n';
        }
    });
}

void EvoClawServer::stop() {
    stop_requested_.store(true);
    server_.stop();

    {
        std::lock_guard<std::mutex> lock(sse_mutex_);
        for (const auto& weak_client : sse_client_refs_) {
            if (auto client = weak_client.lock()) {
                {
                    std::lock_guard<std::mutex> client_lock(client->mutex);
                    client->closed = true;
                }
                client->cv.notify_all();
            }
        }
        cleanup_sse_clients_locked();
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    running_.store(false);
}

void EvoClawServer::setup_routes() {
    server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(dashboard::kDashboardHtml, "text/html; charset=utf-8");
    });

    server_.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        try {
            json status = facade_.get_status();
            std::size_t active_sse_clients = 0;

            {
                std::lock_guard<std::mutex> lock(sse_mutex_);
                cleanup_sse_clients_locked();
                active_sse_clients = sse_clients_.size();
            }

            status["server"] = {
                {"port", port_},
                {"running", running_.load()},
                {"sse_clients", active_sse_clients}
            };

            set_json_response(res, status);
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/agents", [this](const httplib::Request&, httplib::Response& res) {
        try {
            set_json_response(res, facade_.get_capability_matrix());
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/matrix", [this](const httplib::Request&, httplib::Response& res) {
        try {
            set_json_response(res, facade_.get_capability_matrix());
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/logs", [this](const httplib::Request& req, httplib::Response& res) {
        const auto parse_param = [&req](const char* key) -> std::optional<Timestamp> {
            if (!req.has_param(key)) {
                return std::nullopt;
            }
            return parse_iso8601_timestamp(req.get_param_value(key));
        };

        const auto start = parse_param("start");
        if (req.has_param("start") && !start.has_value()) {
            set_json_response(res, build_error("Invalid ISO8601 'start' parameter"), 400);
            return;
        }

        const auto end = parse_param("end");
        if (req.has_param("end") && !end.has_value()) {
            set_json_response(res, build_error("Invalid ISO8601 'end' parameter"), 400);
            return;
        }

        try {
            const auto logs = facade_.query_logs(
                start.value_or(Timestamp::min()),
                end.value_or(Timestamp::max()));

            json payload;
            payload["count"] = logs.size();
            payload["logs"] = json::array();
            for (const auto& entry : logs) {
                payload["logs"].push_back(org_log_entry_to_json(entry));
            }

            if (start.has_value()) {
                payload["start"] = timestamp_to_string(*start);
            }
            if (end.has_value()) {
                payload["end"] = timestamp_to_string(*end);
            }

            set_json_response(res, payload);
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/logs/stats", [this](const httplib::Request& req, httplib::Response& res) {
        const auto parse_param = [&req](const char* key) -> std::optional<Timestamp> {
            if (!req.has_param(key)) {
                return std::nullopt;
            }
            return parse_iso8601_timestamp(req.get_param_value(key));
        };

        const auto start = parse_param("start");
        if (req.has_param("start") && !start.has_value()) {
            set_json_response(res, build_error("Invalid ISO8601 'start' parameter"), 400);
            return;
        }

        const auto end = parse_param("end");
        if (req.has_param("end") && !end.has_value()) {
            set_json_response(res, build_error("Invalid ISO8601 'end' parameter"), 400);
            return;
        }

        try {
            const auto stats = facade_.get_log_stats(
                start.value_or(Timestamp::min()),
                end.value_or(Timestamp::max()));

            json payload = {
                {"total_tasks", stats.total_tasks},
                {"successful_tasks", stats.successful_tasks},
                {"avg_duration_ms", stats.avg_duration_ms},
                {"avg_critic_score", stats.avg_critic_score},
                {"tasks_by_agent", stats.tasks_by_agent}
            };
            if (start.has_value()) {
                payload["start"] = timestamp_to_string(*start);
            }
            if (end.has_value()) {
                payload["end"] = timestamp_to_string(*end);
            }

            set_json_response(res, payload);
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/zones", [this](const httplib::Request&, httplib::Response& res) {
        try {
            set_json_response(res, facade_.get_zone_status());
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/patterns", [this](const httplib::Request&, httplib::Response& res) {
        try {
            set_json_response(res, facade_.get_pattern_status());
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/entropy", [this](const httplib::Request&, httplib::Response& res) {
        try {
            set_json_response(res, facade_.get_entropy_status());
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Post("/api/task", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body.empty() ? "{}" : req.body);
        } catch (...) {
            set_json_response(res, build_error("Invalid JSON body"), 400);
            return;
        }

        if (!body.contains("description") || !body["description"].is_string()) {
            set_json_response(res, build_error("Field 'description' is required"), 400);
            return;
        }

        const std::string type_str = body.value("type", std::string("execute"));
        const auto task_type = parse_task_type(type_str);
        if (!task_type.has_value()) {
            set_json_response(res, build_error("Unsupported task type: " + type_str), 400);
            return;
        }

        agent::Task task;
        task.id = body.value("id", std::string("task-") + generate_uuid());
        task.description = body["description"].get<std::string>();
        task.type = *task_type;
        if (body.contains("context") && body["context"].is_object()) {
            task.context = body["context"];
        }

        try {
            const auto result = facade_.submit_task(task);
            set_json_response(res, {
                {"ok", result.success},
                {"task_id", result.task_id.empty() ? task.id : result.task_id},
                {"agent_id", result.agent_id},
                {"success", result.success},
                {"output", result.output},
                {"confidence", result.confidence},
                {"metadata", result.metadata},
                {"task_type", task_type_to_string(task.type)}
            });
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Post("/api/evolve", [this](const httplib::Request&, httplib::Response& res) {
        try {
            facade_.trigger_evolution();
            json status = facade_.get_status();
            json last_cycle = json::object();
            if (status.contains("evolution") && status["evolution"].is_object()) {
                last_cycle = status["evolution"].value("last_cycle", json::object());
            }
            set_json_response(res, {
                {"ok", true},
                {"evolution", last_cycle}
            });
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Post("/api/matrix/save", [this](const httplib::Request&, httplib::Response& res) {
        try {
            facade_.save_state();
            set_json_response(res, {
                {"ok", true},
                {"message", "Matrix state saved"}
            });
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/budget", [this](const httplib::Request&, httplib::Response& res) {
        try {
            set_json_response(res, facade_.get_budget_report());
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/evolution/budget", [this](const httplib::Request&, httplib::Response& res) {
        try {
            set_json_response(res, facade_.get_evolution_budget_status());
        } catch (const std::exception& ex) {
            set_json_response(res, build_error(ex.what()), 500);
        }
    });

    server_.Get("/api/runtime-config/version", [this](const httplib::Request& req, httplib::Response& res) {
        handle_runtime_config_version(req, res);
    });

    server_.Get("/api/runtime-config/history", [this](const httplib::Request& req, httplib::Response& res) {
        handle_runtime_config_history(req, res);
    });

    server_.Get("/api/runtime-config/diff", [this](const httplib::Request& req, httplib::Response& res) {
        handle_runtime_config_diff(req, res);
    });

    server_.Post("/api/runtime-config/history/prune", [this](const httplib::Request& req, httplib::Response& res) {
        handle_runtime_config_history_prune(req, res);
    });

    server_.Get("/api/runtime-config/governance", [this](const httplib::Request& req, httplib::Response& res) {
        handle_runtime_config_governance_get(req, res);
    });

    server_.Post("/api/runtime-config/governance", [this](const httplib::Request& req, httplib::Response& res) {
        handle_runtime_config_governance_post(req, res);
    });

    server_.Post("/api/runtime-config/rollback", [this](const httplib::Request& req, httplib::Response& res) {
        handle_runtime_config_rollback(req, res);
    });

    server_.Get("/api/events", [this](const httplib::Request&, httplib::Response& res) {
        json payload;
        payload["events"] = json::array();

        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            for (const auto& event : recent_events_) {
                payload["events"].push_back(event);
            }
        }

        payload["count"] = payload["events"].size();
        set_json_response(res, payload);
    });

    server_.Get("/api/events/stream", [this](const httplib::Request&, httplib::Response& res) {
        auto client = std::make_shared<SseClient>();
        auto sender = [weak_client = std::weak_ptr<SseClient>(client)](const std::string& payload) {
            if (const auto locked_client = weak_client.lock()) {
                {
                    std::lock_guard<std::mutex> lock(locked_client->mutex);
                    if (locked_client->closed) {
                        return;
                    }
                    locked_client->queue.push_back(payload);
                }
                locked_client->cv.notify_one();
            }
        };

        {
            std::lock_guard<std::mutex> lock(sse_mutex_);
            sse_clients_.push_back(sender);
            sse_client_refs_.push_back(client);
            cleanup_sse_clients_locked();
        }

        sender(make_sse_payload({
            {"event", "connected"},
            {"timestamp", timestamp_to_string(now())},
            {"message", "SSE stream connected"}
        }));

        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("X-Accel-Buffering", "no");

        res.set_chunked_content_provider(
            "text/event-stream",
            [client](size_t, httplib::DataSink& sink) {
                std::unique_lock<std::mutex> lock(client->mutex);
                if (client->queue.empty()) {
                    client->cv.wait_for(lock, std::chrono::seconds(15), [client]() {
                        return client->closed || !client->queue.empty();
                    });
                }

                if (client->closed) {
                    return false;
                }

                if (client->queue.empty()) {
                    lock.unlock();
                    static constexpr char kKeepAlive[] = ": keep-alive\n\n";
                    return sink.write(kKeepAlive, sizeof(kKeepAlive) - 1);
                }

                std::string payload = std::move(client->queue.front());
                client->queue.pop_front();
                lock.unlock();

                if (sink.is_writable && !sink.is_writable()) {
                    return false;
                }
                return sink.write(payload.data(), payload.size());
            },
            [client](bool) {
                {
                    std::lock_guard<std::mutex> lock(client->mutex);
                    client->closed = true;
                }
                client->cv.notify_all();
            });
    });
}

void EvoClawServer::handle_runtime_config_version(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("agent_id") || req.get_param_value("agent_id").empty()) {
        set_json_response(res, build_error("Query parameter 'agent_id' is required"), 400);
        return;
    }

    try {
        const auto payload = facade_.get_agent_runtime_version(req.get_param_value("agent_id"));
        set_json_response(res, payload, runtime_config_query_status(payload));
    } catch (const std::exception& ex) {
        set_json_response(res, build_error(ex.what()), 500);
    }
}

void EvoClawServer::handle_runtime_config_history(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("agent_id") || req.get_param_value("agent_id").empty()) {
        set_json_response(res, build_error("Query parameter 'agent_id' is required"), 400);
        return;
    }

    try {
        const auto payload = facade_.get_agent_runtime_history(req.get_param_value("agent_id"));
        set_json_response(res, payload, runtime_config_query_status(payload));
    } catch (const std::exception& ex) {
        set_json_response(res, build_error(ex.what()), 500);
    }
}

void EvoClawServer::handle_runtime_config_diff(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("agent_id") || req.get_param_value("agent_id").empty()) {
        set_json_response(res, build_error("Query parameter 'agent_id' is required"), 400);
        return;
    }
    if (!req.has_param("from_version")) {
        set_json_response(res, build_error("Query parameter 'from_version' is required"), 400);
        return;
    }
    if (!req.has_param("to_version")) {
        set_json_response(res, build_error("Query parameter 'to_version' is required"), 400);
        return;
    }

    const auto from_version = parse_uint64_param(req.get_param_value("from_version"));
    if (!from_version.has_value()) {
        set_json_response(res, build_error("Query parameter 'from_version' must be a non-negative integer"), 400);
        return;
    }

    const auto to_version = parse_uint64_param(req.get_param_value("to_version"));
    if (!to_version.has_value()) {
        set_json_response(res, build_error("Query parameter 'to_version' must be a non-negative integer"), 400);
        return;
    }

    try {
        const auto payload = facade_.get_agent_runtime_diff(
            req.get_param_value("agent_id"),
            *from_version,
            *to_version);
        set_json_response(res, payload, runtime_config_query_status(payload));
    } catch (const std::exception& ex) {
        set_json_response(res, build_error(ex.what()), 500);
    }
}

void EvoClawServer::handle_runtime_config_history_prune(const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
        body = json::parse(req.body.empty() ? "{}" : req.body);
    } catch (...) {
        set_json_response(res, build_error("Invalid JSON body"), 400);
        return;
    }

    if (!body.contains("keep_last_per_agent")) {
        set_json_response(res, build_error("Field 'keep_last_per_agent' is required"), 400);
        return;
    }

    const auto keep_last_per_agent = parse_non_negative_size_t(body["keep_last_per_agent"]);
    if (!keep_last_per_agent.has_value()) {
        set_json_response(res, build_error("Field 'keep_last_per_agent' must be a non-negative integer"), 400);
        return;
    }

    try {
        facade_.clear_old_runtime_config_history(*keep_last_per_agent);
        facade_.save_state();

        const auto status = facade_.get_status();
        set_json_response(res, {
            {"ok", true},
            {"keep_last_per_agent", *keep_last_per_agent},
            {"runtime_config", status.value("runtime_config", json::object())}
        });
    } catch (const std::exception& ex) {
        set_json_response(res, build_error(ex.what()), 500);
    }
}

void EvoClawServer::handle_runtime_config_governance_get(const httplib::Request&, httplib::Response& res) {
    try {
        set_json_response(res, facade_.get_runtime_governance_status());
    } catch (const std::exception& ex) {
        set_json_response(res, build_error(ex.what()), 500);
    }
}

void EvoClawServer::handle_runtime_config_governance_post(const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
        body = json::parse(req.body.empty() ? "{}" : req.body);
    } catch (...) {
        set_json_response(res, build_error("Invalid JSON body"), 400);
        return;
    }

    if (!body.contains("keep_last_per_agent")) {
        set_json_response(res, build_error("Field 'keep_last_per_agent' is required"), 400);
        return;
    }

    const auto keep_last_per_agent = parse_non_negative_size_t(body["keep_last_per_agent"]);
    if (!keep_last_per_agent.has_value()) {
        set_json_response(res, build_error("Field 'keep_last_per_agent' must be a non-negative integer"), 400);
        return;
    }

    try {
        facade_.set_runtime_history_keep_last_per_agent(*keep_last_per_agent);
        facade_.save_state();

        const auto status = facade_.get_status();
        set_json_response(res, {
            {"ok", true},
            {"governance", facade_.get_runtime_governance_status()},
            {"runtime_config", status.value("runtime_config", json::object())}
        });
    } catch (const std::exception& ex) {
        set_json_response(res, build_error(ex.what()), 500);
    }
}

void EvoClawServer::handle_runtime_config_rollback(const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
        body = json::parse(req.body.empty() ? "{}" : req.body);
    } catch (...) {
        set_json_response(res, build_error("Invalid JSON body"), 400);
        return;
    }

    if (!body.contains("proposal_id") || !body["proposal_id"].is_string()) {
        set_json_response(res, build_error("Field 'proposal_id' is required and must be a string"), 400);
        return;
    }

    const std::string proposal_id = body["proposal_id"].get<std::string>();
    if (proposal_id.empty()) {
        set_json_response(res, build_error("Field 'proposal_id' cannot be empty"), 400);
        return;
    }

    std::string reason;
    try {
        const bool success = facade_.rollback_proposal(proposal_id, &reason);
        if (!success) {
            set_json_response(res, {
                {"ok", false},
                {"error", reason.empty() ? "Rollback failed" : reason},
                {"proposal_id", proposal_id}
            }, 400);
            return;
        }

        facade_.save_state();

        const auto status = facade_.get_status();
        set_json_response(res, {
            {"ok", true},
            {"proposal_id", proposal_id},
            {"runtime_config", status.value("runtime_config", json::object())}
        });
    } catch (const std::exception& ex) {
        set_json_response(res, build_error(ex.what()), 500);
    }
}

void EvoClawServer::broadcast_event(const nlohmann::json& event) {
    const std::string payload = make_sse_payload(event);

    std::vector<std::function<void(const std::string&)>> clients;
    {
        std::lock_guard<std::mutex> lock(sse_mutex_);
        cleanup_sse_clients_locked();
        clients = sse_clients_;
    }

    for (const auto& sender : clients) {
        sender(payload);
    }
}

void EvoClawServer::on_facade_event(const nlohmann::json& event) {
    if (!event.is_object()) {
        return;
    }

    json enriched = event;
    if (!enriched.contains("timestamp")) {
        enriched["timestamp"] = timestamp_to_string(now());
    }
    if (!enriched.contains("event")) {
        enriched["event"] = "message";
    }
    if (!enriched.contains("message")) {
        enriched["message"] = event_message(enriched);
    }

    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        recent_events_.push_back(enriched);
        while (recent_events_.size() > kMaxRecentEvents) {
            recent_events_.pop_front();
        }
    }

    broadcast_event(enriched);
}

void EvoClawServer::cleanup_sse_clients_locked() {
    if (sse_clients_.size() != sse_client_refs_.size()) {
        sse_clients_.resize(std::min(sse_clients_.size(), sse_client_refs_.size()));
        sse_client_refs_.resize(sse_clients_.size());
    }

    std::size_t write_index = 0;
    for (std::size_t i = 0; i < sse_client_refs_.size(); ++i) {
        bool keep = false;
        if (const auto client = sse_client_refs_[i].lock()) {
            std::lock_guard<std::mutex> lock(client->mutex);
            keep = !client->closed;
        }

        if (keep) {
            if (write_index != i) {
                sse_client_refs_[write_index] = std::move(sse_client_refs_[i]);
                sse_clients_[write_index] = std::move(sse_clients_[i]);
            }
            ++write_index;
        }
    }

    sse_client_refs_.resize(write_index);
    sse_clients_.resize(write_index);
}

std::optional<TaskType> EvoClawServer::parse_task_type(const std::string& type) {
    std::string normalized;
    normalized.reserve(type.size());
    for (const char ch : type) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (normalized == "route") {
        return TaskType::ROUTE;
    }
    if (normalized == "execute") {
        return TaskType::EXECUTE;
    }
    if (normalized == "critique") {
        return TaskType::CRITIQUE;
    }
    if (normalized == "synthesize") {
        return TaskType::SYNTHESIZE;
    }
    if (normalized == "evolve") {
        return TaskType::EVOLVE;
    }

    return std::nullopt;
}

std::string EvoClawServer::task_type_to_string(const TaskType type) {
    switch (type) {
        case TaskType::ROUTE:
            return "route";
        case TaskType::EXECUTE:
            return "execute";
        case TaskType::CRITIQUE:
            return "critique";
        case TaskType::SYNTHESIZE:
            return "synthesize";
        case TaskType::EVOLVE:
            return "evolve";
    }

    return "unknown";
}

std::string EvoClawServer::make_sse_payload(const nlohmann::json& event) {
    const std::string event_name = event.value("event", std::string("message"));
    return "event: " + event_name + "\n" +
           "data: " + event.dump() + "\n\n";
}

} // namespace evoclaw::server
