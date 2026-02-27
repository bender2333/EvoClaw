#include "llm/llm_client.hpp"

#include "httplib.h"

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace evoclaw::llm {
namespace {

using json = nlohmann::json;

struct UrlParts {
    std::string host;
    int port = 80;
    std::string path_prefix;
    bool use_tls = false;
};

std::optional<UrlParts> parse_base_url(const std::string& base_url, std::string* error) {
    static constexpr const char* kHttpPrefix = "http://";
    static constexpr const char* kHttpsPrefix = "https://";

    bool use_tls = false;
    std::string prefix;
    if (base_url.starts_with(kHttpPrefix)) {
        prefix = kHttpPrefix;
        use_tls = false;
    } else if (base_url.starts_with(kHttpsPrefix)) {
        prefix = kHttpsPrefix;
        use_tls = true;
    } else {
        if (error) {
            *error = "base_url must start with http:// or https://";
        }
        return std::nullopt;
    }

    const std::string remainder = base_url.substr(prefix.size());
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
    parts.use_tls = use_tls;
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
        parts.port = use_tls ? 443 : 80;
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

std::optional<json> load_openclaw_config_json() {
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
    return parsed;
}

std::string get_env_string(const char* name) {
    if (const char* value = std::getenv(name); value != nullptr && *value != '\0') {
        return value;
    }
    return {};
}

std::optional<std::string> json_string(const json& object,
                                       std::initializer_list<const char*> keys) {
    if (!object.is_object()) {
        return std::nullopt;
    }
    for (const char* key : keys) {
        if (object.contains(key) && object[key].is_string()) {
            const std::string value = object[key].get<std::string>();
            if (!value.empty()) {
                return value;
            }
        }
    }
    return std::nullopt;
}

std::optional<json> select_provider_config(const json& parsed,
                                           const std::string& preferred_provider) {
    if (!parsed.contains("models") || !parsed["models"].is_object()) {
        return std::nullopt;
    }

    const auto& models = parsed["models"];
    if (!models.contains("providers") || !models["providers"].is_object()) {
        return std::nullopt;
    }

    const auto& providers = models["providers"];
    if (!preferred_provider.empty() && providers.contains(preferred_provider) &&
        providers[preferred_provider].is_object()) {
        return providers[preferred_provider];
    }

    if (providers.contains("bailian") && providers["bailian"].is_object()) {
        return providers["bailian"];
    }

    if (providers.contains("mynewapi") && providers["mynewapi"].is_object()) {
        return providers["mynewapi"];
    }

    for (const auto& [name, provider] : providers.items()) {
        (void)name;
        if (provider.is_object()) {
            return provider;
        }
    }

    return std::nullopt;
}

void apply_provider_config(const json& provider, LLMConfig* config) {
    if (config == nullptr || !provider.is_object()) {
        return;
    }

    if (const auto api_key = json_string(provider, {"apiKey", "api_key"}); api_key.has_value()) {
        config->api_key = *api_key;
    }

    if (const auto base_url = json_string(provider, {"baseUrl", "baseURL", "base_url"});
        base_url.has_value()) {
        config->base_url = *base_url;
    }

    if (provider.contains("models") && provider["models"].is_array() && !provider["models"].empty()) {
        const auto& first = provider["models"][0];
        if (first.is_object()) {
            if (const auto model_id = json_string(first, {"id", "name"}); model_id.has_value()) {
                config->model = *model_id;
            }
        }
    }

    if (const auto model = json_string(provider, {"model"}); model.has_value()) {
        config->model = *model;
    }
}

void apply_openclaw_provider_defaults(LLMConfig* config, const std::string& preferred_provider) {
    if (config == nullptr) {
        return;
    }

    const auto config_json = load_openclaw_config_json();
    if (!config_json.has_value()) {
        return;
    }

    const auto provider = select_provider_config(*config_json, preferred_provider);
    if (!provider.has_value()) {
        return;
    }

    apply_provider_config(*provider, config);
}

template <typename ClientT>
httplib::Result post_json(ClientT& client,
                          const std::string& endpoint,
                          const httplib::Headers& headers,
                          const std::string& body) {
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(30, 0);
    client.set_write_timeout(30, 0);
    return client.Post(endpoint, headers, body, "application/json");
}

LLMResponse parse_chat_payload(const int http_status, const std::string& response_body) {
    if (http_status < 200 || http_status >= 300) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "LLM API returned HTTP " + std::to_string(http_status) + ": " + response_body
        };
    }

    json body;
    try {
        body = json::parse(response_body.empty() ? "{}" : response_body);
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

LLMResponse parse_chat_response(const httplib::Result& response) {
    if (!response) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "HTTP request failed"
        };
    }
    return parse_chat_payload(response->status, response->body);
}

std::string shell_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('\'');
    for (char ch : value) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('\'');
    return escaped;
}

std::optional<std::filesystem::path> create_temp_file_path(const char* prefix) {
    std::string tmpl = std::string("/tmp/") + prefix + "XXXXXX";
    std::vector<char> buffer(tmpl.begin(), tmpl.end());
    buffer.push_back('\0');

    const int fd = mkstemp(buffer.data());
    if (fd < 0) {
        return std::nullopt;
    }
    close(fd);
    return std::filesystem::path(buffer.data());
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return {};
    }

    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

LLMResponse post_json_via_curl(const std::string& base_url,
                               const std::string& endpoint,
                               const std::string& api_key,
                               const std::string& payload) {
    const auto request_path = create_temp_file_path("evoclaw_req_");
    const auto response_path = create_temp_file_path("evoclaw_rsp_");
    const auto error_path = create_temp_file_path("evoclaw_err_");

    if (!request_path.has_value() || !response_path.has_value() || !error_path.has_value()) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "Failed to create temp files for curl request"
        };
    }

    {
        std::ofstream out(*request_path, std::ios::binary);
        if (!out.is_open()) {
            std::filesystem::remove(*request_path);
            std::filesystem::remove(*response_path);
            std::filesystem::remove(*error_path);
            return {
                .success = false,
                .content = "",
                .prompt_tokens = 0,
                .completion_tokens = 0,
                .error = "Failed to write curl request body"
            };
        }
        out << payload;
    }

    std::string full_url = base_url;
    if (!full_url.empty() && full_url.back() == '/' && !endpoint.empty() && endpoint.front() == '/') {
        full_url.pop_back();
    }
    full_url += endpoint;

    const std::string auth_header = "Authorization: Bearer " + api_key;

    const std::string command =
        "curl -sS -X POST " + shell_escape(full_url) +
        " -H " + shell_escape(auth_header) +
        " -H 'Content-Type: application/json'" +
        " --data-binary @" + shell_escape(request_path->string()) +
        " -o " + shell_escape(response_path->string()) +
        " -w '%{http_code}'" +
        " 2> " + shell_escape(error_path->string());

    std::string status_output;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe != nullptr) {
        char buffer[64] = {0};
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            status_output += buffer;
        }
    }
    const int code = pipe == nullptr ? -1 : pclose(pipe);

    const std::string response_body = read_text_file(*response_path);
    const std::string error_body = read_text_file(*error_path);

    std::filesystem::remove(*request_path);
    std::filesystem::remove(*response_path);
    std::filesystem::remove(*error_path);

    if (pipe == nullptr || code != 0) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "curl request failed" + (error_body.empty() ? "" : ": " + error_body)
        };
    }

    const auto trim_start = status_output.find_first_not_of(" \n\r\t");
    if (trim_start == std::string::npos) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "curl did not return a valid HTTP status"
        };
    }
    const auto trim_end = status_output.find_last_not_of(" \n\r\t");
    status_output = status_output.substr(trim_start, trim_end - trim_start + 1U);

    int status = 0;
    try {
        status = std::stoi(status_output);
    } catch (...) {
        return {
            .success = false,
            .content = "",
            .prompt_tokens = 0,
            .completion_tokens = 0,
            .error = "curl did not return a valid HTTP status"
        };
    }

    return parse_chat_payload(status, response_body);
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
    const auto parts = parse_base_url(config_.base_url, &parse_error);
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

    httplib::Headers headers{
        {"Authorization", "Bearer " + config_.api_key}
    };
    const std::string endpoint = endpoint_for(parts->path_prefix, "/chat/completions");

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    if (parts->use_tls) {
        httplib::SSLClient client(parts->host, parts->port);
        client.enable_server_certificate_verification(true);
        const auto response = post_json(client, endpoint, headers, payload.dump());
        return parse_chat_response(response);
    }
#else
    if (parts->use_tls) {
        const std::string scheme = "https://";
        std::string authority = scheme + parts->host;
        if (parts->port != 443) {
            authority += ":" + std::to_string(parts->port);
        }
        return post_json_via_curl(authority, endpoint, config_.api_key, payload.dump());
    }
#endif

    httplib::Client client(parts->host, parts->port);
    const auto response = post_json(client, endpoint, headers, payload.dump());
    return parse_chat_response(response);
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

    const std::string preferred_provider = get_env_string("EVOCLAW_PROVIDER");
    apply_openclaw_provider_defaults(&config, preferred_provider);

    if (const std::string env_base_url = get_env_string("EVOCLAW_BASE_URL"); !env_base_url.empty()) {
        config.base_url = env_base_url;
    }

    if (const std::string env_model = get_env_string("EVOCLAW_MODEL"); !env_model.empty()) {
        config.model = env_model;
    }

    if (const std::string env_key = get_env_string("EVOCLAW_API_KEY"); !env_key.empty()) {
        config.api_key = env_key;
    }

    if (config.api_key.empty()) {
        config.mock_mode = true;
    }

    return LLMClient(std::move(config));
}

} // namespace evoclaw::llm
