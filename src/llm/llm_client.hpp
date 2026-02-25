#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace evoclaw::llm {

struct LLMConfig {
    std::string base_url = "http://localhost:3000/v1";
    std::string api_key;
    std::string model = "claude-opus-4-6";
    double temperature = 0.3;
    int max_tokens = 2048;
    bool mock_mode = false;
};

struct ChatMessage {
    std::string role;
    std::string content;
};

struct LLMResponse {
    bool success = false;
    std::string content;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    std::string error;
};

class LLMClient {
public:
    explicit LLMClient(LLMConfig config);

    [[nodiscard]] LLMResponse chat(const std::vector<ChatMessage>& messages) const;
    [[nodiscard]] LLMResponse ask(const std::string& system_prompt, const std::string& user_message) const;

    [[nodiscard]] const LLMConfig& config() const { return config_; }
    [[nodiscard]] bool is_mock_mode() const { return mock_mode_; }
    [[nodiscard]] bool has_api_key() const { return !config_.api_key.empty(); }
    [[nodiscard]] nlohmann::json status_json() const;

private:
    LLMConfig config_;
    bool mock_mode_ = false;
};

[[nodiscard]] LLMClient create_from_env();

} // namespace evoclaw::llm
