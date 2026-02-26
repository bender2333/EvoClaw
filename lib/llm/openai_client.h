#ifndef EVOCLAW_LLM_OPENAI_CLIENT_H_
#define EVOCLAW_LLM_OPENAI_CLIENT_H_

#include "llm/llm_client.h"
#include "config/config.h"

namespace evoclaw {

/// OpenAI 兼容 LLM 客户端（支持 OpenAI / DeepSeek / 本地 Ollama 等）
class OpenAiClient : public LlmClient {
 public:
  explicit OpenAiClient(const LlmConfig& config);

  std::expected<ChatResponse, Error> Chat(
      const std::vector<ChatMessage>& messages,
      const nlohmann::json& options) override;

  std::expected<ChatResponse, Error> ChatJson(
      const std::vector<ChatMessage>& messages,
      const nlohmann::json& options) override;

 private:
  LlmConfig config_;

  std::expected<ChatResponse, Error> DoRequest(
      const std::vector<ChatMessage>& messages,
      const nlohmann::json& options,
      bool json_mode);
};

}  // namespace evoclaw

#endif  // EVOCLAW_LLM_OPENAI_CLIENT_H_
