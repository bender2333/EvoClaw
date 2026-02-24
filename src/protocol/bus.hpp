#pragma once

#include "protocol/message.hpp"

#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace evoclaw::protocol {

struct RateLimitConfig {
    std::size_t max_messages = 100;
    std::chrono::milliseconds window{1000};
};

class MessageBus {
public:
    using Handler = std::function<void(const Message&)>;

    explicit MessageBus(RateLimitConfig config = {});

    void subscribe(const AgentId& agent_id, Handler handler);
    void unsubscribe(const AgentId& agent_id);

    // 发送给消息中的 receivers。
    [[nodiscard]] bool send(const Message& msg);

    // 广播给当前所有订阅者，返回实际分发数量。
    [[nodiscard]] std::size_t broadcast(const Message& msg);

    [[nodiscard]] std::size_t subscriber_count() const;
    [[nodiscard]] bool has_seen(const MessageId& message_id) const;

private:
    [[nodiscard]] bool allow_sender(const AgentId& sender, std::chrono::steady_clock::time_point now_tp);
    [[nodiscard]] bool deduplicate(const MessageId& id, std::chrono::steady_clock::time_point now_tp);
    [[nodiscard]] std::vector<Handler> collect_handlers(const std::vector<AgentId>& receivers) const;

    mutable std::shared_mutex handlers_mutex_;
    std::unordered_map<AgentId, Handler> handlers_;

    mutable std::mutex state_mutex_;
    std::unordered_set<MessageId> seen_ids_;
    std::deque<std::pair<MessageId, std::chrono::steady_clock::time_point>> seen_order_;
    std::unordered_map<AgentId, std::deque<std::chrono::steady_clock::time_point>> sender_windows_;

    RateLimitConfig config_;
    std::chrono::minutes dedup_ttl_{10};
};

} // namespace evoclaw::protocol
