#pragma once

#include "facade/facade.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "httplib.h"

namespace evoclaw::server {

class EvoClawServer {
public:
    explicit EvoClawServer(facade::EvoClawFacade& facade, int port = 8080);
    ~EvoClawServer();

    void start();
    void start_async();
    void stop();
    void handle_runtime_config_version(const httplib::Request& req, httplib::Response& res);
    void handle_runtime_config_history(const httplib::Request& req, httplib::Response& res);
    void handle_runtime_config_diff(const httplib::Request& req, httplib::Response& res);
    void handle_runtime_config_history_prune(const httplib::Request& req, httplib::Response& res);
    void handle_runtime_config_governance_get(const httplib::Request& req, httplib::Response& res);
    void handle_runtime_config_governance_post(const httplib::Request& req, httplib::Response& res);
    void handle_runtime_config_rollback(const httplib::Request& req, httplib::Response& res);

private:
    struct SseClient {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::string> queue;
        bool closed = false;
    };

    void setup_routes();
    void broadcast_event(const nlohmann::json& event);
    void on_facade_event(const nlohmann::json& event);
    void cleanup_sse_clients_locked();

    static std::optional<TaskType> parse_task_type(const std::string& type);
    static std::string task_type_to_string(TaskType type);
    static std::string make_sse_payload(const nlohmann::json& event);

    facade::EvoClawFacade& facade_;
    httplib::Server server_;
    int port_ = 8080;

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread server_thread_;

    std::mutex sse_mutex_;
    std::vector<std::function<void(const std::string&)>> sse_clients_;
    std::vector<std::weak_ptr<SseClient>> sse_client_refs_;

    std::mutex events_mutex_;
    std::deque<nlohmann::json> recent_events_;

    static constexpr std::size_t kMaxRecentEvents = 100;
};

} // namespace evoclaw::server
