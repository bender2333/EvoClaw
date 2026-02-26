#include "facade/facade.h"
#include "common/uuid.h"
#include <chrono>

namespace evoclaw {

Facade::Facade(LlmClient* llm, MessageBus* bus, MemoryStore* memory)
    : llm_(llm), bus_(bus), memory_(memory),
      persona_prompt_("你是 EvoClaw，一个智能管家。用简洁友好的语气回复用户。") {}

std::expected<std::string, Error> Facade::HandleUserMessage(
    const std::string& user_id, const std::string& content) {
  // 1. 记录用户消息到会话历史
  {
    std::lock_guard lock(mutex_);
    auto& history = sessions_[user_id];
    history.push_back({"user", content});
    // 限制历史长度
    if (static_cast<int>(history.size()) > kMaxSessionHistory) {
      history.erase(history.begin());
    }
  }

  // 2. 构建 LLM 请求（含人格 prompt + 会话历史）
  std::vector<ChatMessage> messages;
  messages.push_back({"system", persona_prompt_});

  {
    std::lock_guard lock(mutex_);
    auto& history = sessions_[user_id];
    for (const auto& msg : history) {
      messages.push_back(msg);
    }
  }

  // 3. 调用 LLM 生成回复
  auto response = llm_->Chat(messages);
  if (!response) {
    return std::unexpected(response.error());
  }

  // 4. 记录助手回复到会话历史
  {
    std::lock_guard lock(mutex_);
    auto& history = sessions_[user_id];
    history.push_back({"assistant", response->content});
    if (static_cast<int>(history.size()) > kMaxSessionHistory) {
      history.erase(history.begin());
    }
  }

  // 5. 发布事件到消息总线
  if (bus_) {
    Message event = BuildInternalMessage(user_id, content);
    event.msg_type = "facade.user_message";
    event.payload["response"] = response->content;
    event.payload["tokens"] = response->total_tokens;
    bus_->Publish("facade", event);
  }

  return response->content;
}

std::expected<std::string, Error> Facade::FormatResponse(const std::string& raw_response) {
  std::vector<ChatMessage> messages;
  messages.push_back({"system", persona_prompt_ +
      "\n请将以下内部系统输出改写为面向用户的友好回复，保持信息完整但语气统一："});
  messages.push_back({"user", raw_response});

  auto response = llm_->Chat(messages);
  if (!response) {
    return std::unexpected(response.error());
  }
  return response->content;
}

nlohmann::json Facade::GetSessionContext(const std::string& user_id) const {
  std::lock_guard lock(mutex_);
  nlohmann::json ctx;
  if (auto it = sessions_.find(user_id); it != sessions_.end()) {
    ctx["message_count"] = it->second.size();
    ctx["messages"] = nlohmann::json::array();
    for (const auto& msg : it->second) {
      ctx["messages"].push_back({{"role", msg.role}, {"content", msg.content}});
    }
  } else {
    ctx["message_count"] = 0;
    ctx["messages"] = nlohmann::json::array();
  }
  return ctx;
}

size_t Facade::GetActiveSessionCount() const {
  std::lock_guard lock(mutex_);
  return sessions_.size();
}

Message Facade::BuildInternalMessage(const std::string& user_id, const std::string& content) {
  Message msg;
  msg.msg_id = GenerateUuid();
  msg.source = "facade";
  msg.target = "";
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%FT%TZ", std::gmtime(&time_t));
  msg.timestamp = buf;
  msg.payload = {{"user_id", user_id}, {"content", content}};
  return msg;
}

}  // namespace evoclaw
