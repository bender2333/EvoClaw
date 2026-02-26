#ifndef EVOCLAW_LLM_LLM_CLIENT_H_
#define EVOCLAW_LLM_LLM_CLIENT_H_

#include "config/config.h"
#include <expected>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace evoclaw {

/// LLM 对话消息
struct ChatMessage {
  std::string role;     // "system", "user", "assistant"
  std::string content;
};

/// LLM 响应
struct ChatResponse {
  std::string content;
  int prompt_tokens = 0;
  int completion_tokens = 0;
  int total_tokens = 0;
};

/// LLM 客户端纯虚接口
class LlmClient {
 public:
  virtual ~LlmClient() = default;

  /// 发送对话请求
  virtual std::expected<ChatResponse, Error> Chat(
      const std::vector<ChatMessage>& messages,
      const nlohmann::json& options = {}) = 0;

  /// 发送对话请求（带 JSON 输出约束）
  virtual std::expected<ChatResponse, Error> ChatJson(
      const std::vector<ChatMessage>& messages,
      const nlohmann::json& options = {}) = 0;
};

}  // namespace evoclaw

#endif  // EVOCLAW_LLM_LLM_CLIENT_H_
