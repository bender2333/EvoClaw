#include "llm/openai_client.h"
#include <httplib.h>

namespace evoclaw {

OpenAiClient::OpenAiClient(const LlmConfig& config) : config_(config) {}

std::expected<ChatResponse, Error> OpenAiClient::DoRequest(
    const std::vector<ChatMessage>& messages,
    const nlohmann::json& options,
    bool json_mode) {
  try {
    // 构建请求体
    nlohmann::json body;
    body["model"] = config_.model;
    body["messages"] = nlohmann::json::array();
    for (const auto& msg : messages) {
      body["messages"].push_back({{"role", msg.role}, {"content", msg.content}});
    }
    if (json_mode) {
      body["response_format"] = {{"type", "json_object"}};
    }
    // 合并额外选项
    for (auto& [key, val] : options.items()) {
      body[key] = val;
    }

    // HTTP 请求
    httplib::Client cli(config_.base_url);
    cli.set_connection_timeout(config_.timeout_seconds);
    cli.set_read_timeout(config_.timeout_seconds);

    httplib::Headers headers = {
        {"Authorization", "Bearer " + config_.api_key},
        {"Content-Type", "application/json"},
    };

    int retries = 0;
    while (retries <= config_.max_retries) {
      auto res = cli.Post("/chat/completions", headers, body.dump(), "application/json");
      if (res && res->status == 200) {
        auto j = nlohmann::json::parse(res->body);
        ChatResponse response;
        response.content = j["choices"][0]["message"]["content"].get<std::string>();
        if (j.contains("usage")) {
          response.prompt_tokens = j["usage"].value("prompt_tokens", 0);
          response.completion_tokens = j["usage"].value("completion_tokens", 0);
          response.total_tokens = j["usage"].value("total_tokens", 0);
        }
        return response;
      }

      ++retries;
      if (retries > config_.max_retries) {
        std::string err_msg = res ? "HTTP " + std::to_string(res->status) + ": " + res->body
                                  : "Connection failed";
        return std::unexpected(Error{Error::Code::kInternal, err_msg, "OpenAiClient::DoRequest"});
      }
    }

    return std::unexpected(Error{Error::Code::kInternal, "Max retries exceeded", "OpenAiClient::DoRequest"});
  } catch (const std::exception& e) {
    return std::unexpected(Error{Error::Code::kInternal, e.what(), "OpenAiClient::DoRequest"});
  }
}

std::expected<ChatResponse, Error> OpenAiClient::Chat(
    const std::vector<ChatMessage>& messages,
    const nlohmann::json& options) {
  return DoRequest(messages, options, false);
}

std::expected<ChatResponse, Error> OpenAiClient::ChatJson(
    const std::vector<ChatMessage>& messages,
    const nlohmann::json& options) {
  return DoRequest(messages, options, true);
}

}  // namespace evoclaw
