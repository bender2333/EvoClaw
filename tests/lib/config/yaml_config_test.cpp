#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "config/yaml_config.h"

namespace evoclaw::config {
namespace {

namespace fs = std::filesystem;

/// 测试用临时 YAML 文件辅助类
class YamlConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tmp_dir_ = fs::temp_directory_path() / "evoclaw_config_test";
    fs::create_directories(tmp_dir_);
  }

  void TearDown() override {
    fs::remove_all(tmp_dir_);
    // 清理可能设置的环境变量
    UnsetEnv("EVOCLAW_BUS_ENGINE_PUB");
    UnsetEnv("EVOCLAW_LOG_LEVEL");
    UnsetEnv("EVOCLAW_LLM_MAX_RETRIES");
    UnsetEnv("EVOCLAW_DISCORD_ENABLED");
  }

  /// 写入临时 YAML 文件并返回路径
  std::string WriteYaml(std::string_view content,
                        std::string_view filename = "test.yaml") {
    auto path = tmp_dir_ / filename;
    std::ofstream ofs(path);
    ofs << content;
    ofs.close();
    return path.string();
  }

  static void SetEnv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
  }

  static void UnsetEnv(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
  }

  fs::path tmp_dir_;
};

// ── 场景 1：正常加载 ──────────────────────────────────────

TEST_F(YamlConfigTest, Load_ValidFile_ReturnsConfig) {
  auto path = WriteYaml(R"(
bus:
  engine_pub: "ipc:///tmp/engine-pub.sock"
  engine_router: "ipc:///tmp/engine-router.sock"
log:
  level: "debug"
llm:
  max_retries: 3
  timeout_seconds: 30
discord:
  enabled: true
)");

  auto result = YamlConfig::Load(path);
  ASSERT_TRUE(result.has_value()) << result.error().message;
}

TEST_F(YamlConfigTest, GetString_DottedKey_ReturnsValue) {
  auto path = WriteYaml(R"(
bus:
  engine_pub: "ipc:///tmp/engine-pub.sock"
)");

  auto config = YamlConfig::Load(path);
  ASSERT_TRUE(config.has_value());

  auto val = config->GetString("bus.engine_pub");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, "ipc:///tmp/engine-pub.sock");
}

TEST_F(YamlConfigTest, GetInt_ValidKey_ReturnsValue) {
  auto path = WriteYaml(R"(
llm:
  max_retries: 5
)");

  auto config = YamlConfig::Load(path);
  ASSERT_TRUE(config.has_value());

  auto val = config->GetInt("llm.max_retries");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, 5);
}

TEST_F(YamlConfigTest, GetBool_ValidKey_ReturnsValue) {
  auto path = WriteYaml(R"(
discord:
  enabled: true
)");

  auto config = YamlConfig::Load(path);
  ASSERT_TRUE(config.has_value());

  auto val = config->GetBool("discord.enabled");
  ASSERT_TRUE(val.has_value());
  EXPECT_TRUE(*val);
}

TEST_F(YamlConfigTest, GetStringOr_MissingKey_ReturnsDefault) {
  auto path = WriteYaml("bus:\n  engine_pub: test\n");

  auto config = YamlConfig::Load(path);
  ASSERT_TRUE(config.has_value());

  EXPECT_EQ(config->GetStringOr("nonexistent.key", "fallback"), "fallback");
}

TEST_F(YamlConfigTest, GetIntOr_MissingKey_ReturnsDefault) {
  auto path = WriteYaml("bus:\n  engine_pub: test\n");

  auto config = YamlConfig::Load(path);
  ASSERT_TRUE(config.has_value());

  EXPECT_EQ(config->GetIntOr("nonexistent.key", 42), 42);
}

// ── 场景 2：缺失文件 ──────────────────────────────────────

TEST_F(YamlConfigTest, Load_MissingFile_ReturnsNotFoundError) {
  auto result = YamlConfig::Load("/nonexistent/path/config.yaml");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ErrorCode::kNotFound);
  EXPECT_NE(result.error().context.find("YamlConfig::Load"), std::string::npos);
}

// ── 场景 3：格式错误 ──────────────────────────────────────

TEST_F(YamlConfigTest, Load_MalformedYaml_ReturnsSerializationError) {
  auto path = WriteYaml("{{{{invalid yaml: [[[");

  auto result = YamlConfig::Load(path);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ErrorCode::kSerializationError);
}

TEST_F(YamlConfigTest, GetString_MissingKey_ReturnsNotFoundError) {
  auto path = WriteYaml("bus:\n  engine_pub: test\n");

  auto config = YamlConfig::Load(path);
  ASSERT_TRUE(config.has_value());

  auto val = config->GetString("nonexistent.deep.key");
  ASSERT_FALSE(val.has_value());
  EXPECT_EQ(val.error().code, ErrorCode::kNotFound);
}

// ── 环境变量覆盖 ──────────────────────────────────────────

TEST_F(YamlConfigTest, GetString_EnvOverride_TakesPrecedence) {
  auto path = WriteYaml(R"(
bus:
  engine_pub: "from_yaml"
)");

  SetEnv("EVOCLAW_BUS_ENGINE_PUB", "from_env");

  auto config = YamlConfig::Load(path);
  ASSERT_TRUE(config.has_value());

  auto val = config->GetString("bus.engine_pub");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, "from_env");
}

TEST_F(YamlConfigTest, GetInt_EnvOverride_ParsesInteger) {
  auto path = WriteYaml("llm:\n  max_retries: 3\n");

  SetEnv("EVOCLAW_LLM_MAX_RETRIES", "10");

  auto config = YamlConfig::Load(path);
  ASSERT_TRUE(config.has_value());

  auto val = config->GetInt("llm.max_retries");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, 10);
}

TEST_F(YamlConfigTest, GetBool_EnvOverride_ParsesBool) {
  auto path = WriteYaml("discord:\n  enabled: false\n");

  SetEnv("EVOCLAW_DISCORD_ENABLED", "true");

  auto config = YamlConfig::Load(path);
  ASSERT_TRUE(config.has_value());

  auto val = config->GetBool("discord.enabled");
  ASSERT_TRUE(val.has_value());
  EXPECT_TRUE(*val);
}

}  // namespace
}  // namespace evoclaw::config
