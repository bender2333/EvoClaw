#include "protocol/bus.hpp"

#include <utility>

namespace evoclaw::protocol {

MessageBus::MessageBus(RateLimitConfig config)
    : config_(config) {}

void MessageBus::subscribe(const AgentId& agent_id, Handler handler) {
    std::unique_lock lock(handlers_mutex_);
    handlers_[agent_id] = std::move(handler);
}

void MessageBus::unsubscribe(const AgentId& agent_id) {
    std::unique_lock lock(handlers_mutex_);
    handlers_.erase(agent_id);
}

bool MessageBus::send(const Message& msg) {
    if (msg.receivers.empty()) {
        return false;
    }

    const auto now_tp = std::chrono::steady_clock::now();
    {
        std::scoped_lock lock(state_mutex_);
        if (!deduplicate(msg.id, now_tp)) {
            return false;
        }
        if (!allow_sender(msg.sender, now_tp)) {
            return false;
        }
    }

    const auto handlers = collect_handlers(msg.receivers);
    for (const auto& handler : handlers) {
        handler(msg);
    }

    return !handlers.empty();
}

std::size_t MessageBus::broadcast(const Message& msg) {
    const auto now_tp = std::chrono::steady_clock::now();
    {
        std::scoped_lock lock(state_mutex_);
        if (!deduplicate(msg.id, now_tp)) {
            return 0;
        }
        if (!allow_sender(msg.sender, now_tp)) {
            return 0;
        }
    }

    std::vector<Handler> handlers;
    {
        std::shared_lock lock(handlers_mutex_);
        handlers.reserve(handlers_.size());
        for (const auto& [agent_id, handler] : handlers_) {
            (void)agent_id;
            handlers.push_back(handler);
        }
    }

    for (const auto& handler : handlers) {
        handler(msg);
    }

    return handlers.size();
}

std::size_t MessageBus::subscriber_count() const {
    std::shared_lock lock(handlers_mutex_);
    return handlers_.size();
}

bool MessageBus::has_seen(const MessageId& message_id) const {
    std::scoped_lock lock(state_mutex_);
    return seen_ids_.contains(message_id);
}

bool MessageBus::allow_sender(const AgentId& sender, const std::chrono::steady_clock::time_point now_tp) {
    if (sender.empty()) {
        return true;
    }

    auto& window = sender_windows_[sender];
    while (!window.empty() && (now_tp - window.front()) > config_.window) {
        window.pop_front();
    }

    if (window.size() >= config_.max_messages) {
        return false;
    }

    window.push_back(now_tp);
    return true;
}

bool MessageBus::deduplicate(const MessageId& id, const std::chrono::steady_clock::time_point now_tp) {
    while (!seen_order_.empty() && (now_tp - seen_order_.front().second) > dedup_ttl_) {
        seen_ids_.erase(seen_order_.front().first);
        seen_order_.pop_front();
    }

    if (seen_ids_.contains(id)) {
        return false;
    }

    seen_ids_.insert(id);
    seen_order_.emplace_back(id, now_tp);
    return true;
}

std::vector<MessageBus::Handler> MessageBus::collect_handlers(const std::vector<AgentId>& receivers) const {
    std::vector<Handler> handlers;
    std::unordered_set<AgentId> unique_receivers;
    unique_receivers.reserve(receivers.size());

    std::shared_lock lock(handlers_mutex_);
    for (const auto& receiver : receivers) {
        if (!unique_receivers.insert(receiver).second) {
            continue;
        }
        const auto it = handlers_.find(receiver);
        if (it != handlers_.end()) {
            handlers.push_back(it->second);
        }
    }

    return handlers;
}

} // namespace evoclaw::protocol
