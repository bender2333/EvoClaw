#include "server/server.hpp"

#include "core/types.hpp"
#include "server/dashboard.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>
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
