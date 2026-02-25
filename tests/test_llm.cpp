#include "llm/llm_client.hpp"
#include "core/types.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class ScopedEnvVar {
public:
    explicit ScopedEnvVar(std::string key)
        : key_(std::move(key)) {
        const char* existing = std::getenv(key_.c_str());
        if (existing != nullptr) {
            had_value_ = true;
            old_value_ = existing;
        }
    }

    ~ScopedEnvVar() {
        if (had_value_) {
            setenv(key_.c_str(), old_value_.c_str(), 1);
        } else {
            unsetenv(key_.c_str());
        }
    }

private:
    std::string key_;
    bool had_value_ = false;
    std::string old_value_;
};

TEST(LLMTest, LLMConfigDefaults) {
    evoclaw::llm::LLMConfig config;
    EXPECT_EQ(config.base_url, "http://localhost:3000/v1");
    EXPECT_EQ(config.model, "claude-opus-4-6");
    EXPECT_DOUBLE_EQ(config.temperature, 0.3);
    EXPECT_EQ(config.max_tokens, 2048);
    EXPECT_TRUE(config.api_key.empty());
}

TEST(LLMTest, ChatMessageConstruction) {
    evoclaw::llm::ChatMessage message{"system", "You are a test model."};
    EXPECT_EQ(message.role, "system");
    EXPECT_EQ(message.content, "You are a test model.");
}

TEST(LLMTest, CreateFromEnvFallsBackToMockWhenNoApiKey) {
    ScopedEnvVar api_key_guard("EVOCLAW_API_KEY");
    ScopedEnvVar home_guard("HOME");

    unsetenv("EVOCLAW_API_KEY");
    const auto temp_home = std::filesystem::temp_directory_path() / ("evoclaw_llm_" + evoclaw::generate_uuid());
    std::filesystem::create_directories(temp_home);
    setenv("HOME", temp_home.string().c_str(), 1);

    const auto client = evoclaw::llm::create_from_env();
    EXPECT_TRUE(client.is_mock_mode());
    EXPECT_FALSE(client.has_api_key());

    std::error_code ec;
    std::filesystem::remove_all(temp_home, ec);
}

TEST(LLMTest, MockFallbackResponseWhenApiKeyMissing) {
    evoclaw::llm::LLMConfig config;
    config.api_key.clear();

    const evoclaw::llm::LLMClient client(config);
    const auto response = client.ask("System prompt", "Hello");

    EXPECT_TRUE(response.success);
    EXPECT_FALSE(response.content.empty());
    EXPECT_NE(response.content.find("Mock"), std::string::npos);
}

TEST(LLMTest, CreateFromEnvReadsOpenClawConfigFile) {
    ScopedEnvVar api_key_guard("EVOCLAW_API_KEY");
    ScopedEnvVar home_guard("HOME");

    unsetenv("EVOCLAW_API_KEY");
    const auto temp_home = std::filesystem::temp_directory_path() / ("evoclaw_llm_cfg_" + evoclaw::generate_uuid());
    const auto config_dir = temp_home / ".openclaw";
    std::filesystem::create_directories(config_dir);

    std::ofstream out(config_dir / "openclaw.json");
    out << R"({
  "models": {
    "providers": {
      "mynewapi": {
        "apiKey": "from-config-file"
      }
    }
  }
})";
    out.close();

    setenv("HOME", temp_home.string().c_str(), 1);
    const auto client = evoclaw::llm::create_from_env();
    EXPECT_FALSE(client.is_mock_mode());
    EXPECT_TRUE(client.has_api_key());

    std::error_code ec;
    std::filesystem::remove_all(temp_home, ec);
}

} // namespace
