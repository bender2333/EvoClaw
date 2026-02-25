#include "llm/llm_client.hpp"

#include "httplib.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <utility>

namespace evoclaw::llm {
namespace {

using json = nlohmann::json;

struct UrlParts {
    std::string host;
    int port = 80;
    std::string path_prefix;
};

std::optional<UrlParts> parse_http_base_url(const std::string& base_url, std::string* error) {
    static constexpr const char* kHttpPrefix = "http://";
    if (!base_url.starts_with(kHttpPrefix)) {
        if (error) {
            *error = "base_url must start with http://";
        }
        return std::nullopt;
    }

    const std::string remainder = base_url.substr(std::char_traits<char>::length(kHttpPrefix));
    const std::size_t slash_pos = remainder.find('/');
    const std::string host_port = remainder.substr(0, slash_pos);
    std::string path_prefix = slash_pos == std::string::npos ? "" : remainder.substr(slash_pos);

    if (host_port.empty()) {
        if (error) {
            *error = "base_url host is empty";
        }
        return std::nullopt;
    }

    UrlParts parts;
    std::size_t colon_pos = host_port.rfind(':');
    if (colon_pos != std::string::npos && colon_pos + 1U < host_port.size()) {
        parts.host = host_port.substr(0, colon_pos);
        const std::string port_str = host_port.substr(colon_pos + 1U);
        try {
            const int parsed_port = std::stoi(port_str);
            if (parsed_port <= 0 || parsed_port > 65535) {
                if (error) {
                    *error = "base_url port out of range";
                }
                return std::nullopt;
            }
            parts.port = parsed_port;
        } catch (...) {
            if (error) {
                *error = "base_url port is invalid";
            }
            return std::nullopt;
        }
    } else {
        parts.host = host_port;
    }

    if (parts.host.empty()) {
        if (error) {
            *error = "base_url host is empty";
        }
        return std::nullopt;
    }

    if (!path_prefix.empty() && path_prefix.back() == '/') {
        path_prefix.pop_back();
    }
    parts.path_prefix = std::move(path_prefix);
    return parts;
}

std::string endpoint_for(const std::string& path_prefix, const std::string& suffix) {
    if (path_prefix.empty()) {
        return suffix;
    }
    if (suffix.empty()) {
        return path_prefix;
    }
    if (path_prefix.back() == '/' && suffix.front() == '/') {
        return path_prefix.substr(0, path_prefix.size() - 1U) + suffix;
    }
    if (path_prefix.back() != '/' && suffix.front() != '/') {
        return path_prefix + "/" + suffix;
    }
    return path_prefix + suffix;
}

std::string extract_content_text(const json& content) {
    if (content.is_string()) {
        return content.get<std::string>();
    }
    if (content.is_array()) {
        std::ostringstream oss;
        bool first = true;
        for (const auto& item : content) {
            std::string piece;
            if (item.is_object() && item.contains("text") && item["text"].is_string()) {
                piece = item["text"].get<std::string>();
            } else if (item.is_string()) {
                piece = item.get<std::string>();
            }
            if (!piece.empty()) {
                if (!first) {
                    oss << '\n';
                }
                oss << piece;
                first = false;
            }
        }
        return oss.str();
    }
    return {};
}

std::optional<std::string> api_key_from_openclaw_config() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') {
        return std::nullopt;
    }

    const std::filesystem::path config_path = std::filesystem::path(home) / ".openclaw" / "openclaw.json";
    std::ifstream in(config_path);
    if (!in.is_open()) {
        return std::nullopt;
    }

    json parsed;
    try {
        in >> parsed;
    } catch (...) {
        return std::nullopt;
    }

    if (!parsed.is_object()) {
        return std::nullopt;
    }
    if (!parsed.contains("models") || !parsed["models"].is_object()) {
        return std::nullopt;
    }

    const auto& models = parsed["models"];
    if (!models.contains("providers") || !models["providers"].is_object()) {
        return std::nullopt;
    }

    const auto& providers = models["providers"];
    if (!providers.contains("mynewapi") || !providers["mynewapi"].is_object()) {
        return std::nullopt;
    }

    const auto& mynewapi = providers["mynewapi"];
    if (!mynewapi.contains("apiKey") || !mynewapi["apiKey"].is_string()) {
        return std::nullopt;
    }

    const std::string api_key = mynewapi["apiKey"].get<std::string>();
    if (api_key.empty()) {
        return std::nullopt;
    }
    return api_key;
}

LLMResponse mock_response() {
    return {
        .success = true,
        .content = "Mock LLM response: API key is not configured, running in fallback mode.",
        .prompt_tokens = 0,
        .completion_tokens = 0,
        .error = ""
    };
}

} // namespace

LLMClient::LLMClient(LLMConfig config)
    : config_(std::move(config)) {
    if (config_.base_url.empty()) {
        config_.base_url = "http://localhost:3000/v1";
    }
    if (config_.model.empty()) {
        config_.model = "claude-opus-4-6";
    }
    mock_mode_ = config_.mock_mode || config_.api_key.empty();

    if (mock_mode_) {
        std::cerr << "[EvoClaw] LLM client running in mock mode (missing API key)." << '\n';
    }
}

LLMResponse LLMClient::chat(const std::vector<ChatMessage>& messages) const {
    if (mock_mode_) {
        return mock_response();
    }

    std::string parse_error;
    const auto parts = parse_http_base_url(config_.base_url, &parse_error);
    if (!parts.has_value()) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "Invalid base URL: " + parse_error
        };
    }

    json payload;
    payload["model"] = config_.model;
    payload["temperature"] = config_.temperature;
    payload["max_tokens"] = config_.max_tokens;
    payload["messages"] = json::array();
    for (const auto& message : messages) {
        payload["messages"].push_back({
            {"role", message.role},
            {"content", message.content}
        });
    }

    httplib::Client client(parts->host, parts->port);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(30, 0);
    client.set_write_timeout(30, 0);

    httplib::Headers headers{
        {"Authorization", "Bearer " + config_.api_key}
    };
    const std::string endpoint = endpoint_for(parts->path_prefix, "/chat/completions");
    const auto response = client.Post(endpoint, headers, payload.dump(), "application/json");

    if (!response) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "HTTP request failed"
        };
    }

    if (response->status < 200 || response->status >= 300) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "LLM API returned HTTP " + std::to_string(response->status) + ": " + response->body
        };
    }

    json body;
    try {
        body = json::parse(response->body.empty() ? "{}" : response->body);
    } catch (const std::exception& ex) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = std::string("Failed to parse LLM response JSON: ") + ex.what()
        };
    }

    if (!body.contains("choices") || !body["choices"].is_array() || body["choices"].empty()) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "LLM response is missing choices"
        };
    }

    const auto& first_choice = body["choices"][0];
    if (!first_choice.contains("message") || !first_choice["message"].is_object()) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "LLM response is missing choices[0].message"
        };
    }

    const auto& message = first_choice["message"];
    if (!message.contains("content")) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "LLM response is missing choices[0].message.content"
        };
    }

    const std::string content = extract_content_text(message["content"]);
    if (content.empty()) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "LLM returned empty content"
        };
    }

    LLMResponse result;
    result.success = true;
    result.content = content;
    if (body.contains("usage") && body["usage"].is_object()) {
        result.prompt_tokens = body["usage"].value("prompt_tokens", 0);
        result.completion_tokens = body["usage"].value("completion_tokens", 0);
    }
    return result;
}

LLMResponse LLMClient::ask(const std::string& system_prompt, const std::string& user_message) const {
    return chat({
        ChatMessage{"system", system_prompt},
        ChatMessage{"user", user_message}
    });
}

nlohmann::json LLMClient::status_json() const {
    return {
        {"base_url", config_.base_url},
        {"model", config_.model},
        {"has_api_key", has_api_key()},
        {"mock_mode", mock_mode_},
        {"temperature", config_.temperature},
        {"max_tokens", config_.max_tokens}
    };
}

LLMClient create_from_env() {
    LLMConfig config;

    if (const char* env_key = std::getenv("EVOCLAW_API_KEY"); env_key != nullptr && *env_key != '\0') {
        config.api_key = env_key;
    }

    if (config.api_key.empty()) {
        if (const auto file_key = api_key_from_openclaw_config(); file_key.has_value()) {
            config.api_key = *file_key;
        }
    }

    if (config.api_key.empty()) {
        config.mock_mode = true;
    }

    return LLMClient(std::move(config));
}

} // namespace evoclaw::llm
