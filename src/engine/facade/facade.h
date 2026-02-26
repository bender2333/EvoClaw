#ifndef EVOCLAW_ENGINE_FACADE_FACADE_H_
#define EVOCLAW_ENGINE_FACADE_FACADE_H_

#include "config/config.h"
#include "protocol/message.h"
#include "llm/llm_client.h"
#include "bus/message_bus.h"
#include "memory/memory_store.h"
#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace evoclaw {

/// Facade — 统一人格层
/// 用户永远只看到一个统一人格的管家，内部组织对用户完全透明。
/// 职责：接收用户消息 → 格式化为内部 Message → 分发 → 收集结果 → 人格化输出
class Facade {
 public:
  /// @param llm LLM 客户端（用于人格化输出）
  /// @param bus 消息总线（用于分发任务）
  /// @param memory 记忆存储（用于会话上下文）
  Facade(LlmClient* llm, MessageBus* bus, MemoryStore* memory);

  /// 处理用户消息，返回人格化回复
  std::expected<std::string, Error> HandleUserMessage(
      const std::string& user_id, const std::string& content);

  /// 将原始响应通过 LLM 统一人格化
  std::expected<std::string, Error> FormatResponse(const std::string& raw_response);

  /// 获取用户会话上下文
  nlohmann::json GetSessionContext(const std::string& user_id) const;

  /// 设置系统人格 prompt
  void SetPersonaPrompt(const std::string& prompt) { persona_prompt_ = prompt; }

  /// 获取活跃会话数
  size_t GetActiveSessionCount() const;

 private:
  LlmClient* llm_;
  MessageBus* bus_;
  MemoryStore* memory_;
  std::string persona_prompt_;

  mutable std::mutex mutex_;
  // user_id -> 会话历史（最近 N 条）
  std::unordered_map<std::string, std::vector<ChatMessage>> sessions_;

  static constexpr int kMaxSessionHistory = 20;

  /// 构建内部 Message
  Message BuildInternalMessage(const std::string& user_id, const std::string& content);
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_FACADE_FACADE_H_
