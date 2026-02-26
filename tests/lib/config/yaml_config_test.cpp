#include <gtest/gtest.h>
#include "config/yaml_config.h"
#include "common/uuid.h"
#include <filesystem>
#include <fstream>

using namespace evoclaw;

TEST(YamlConfigTest, LoadValidConfig) {
  auto test_dir = std::filesystem::temp_directory_path() / ("evoclaw_cfg_" + GenerateUuid());
  std::filesystem::create_directories(test_dir);
  auto config_path = test_dir / "test.yaml";

  std::ofstream ofs(config_path);
  ofs << R"(
process_name: test-engine
data_dir: /tmp/evoclaw-test
log_level: DEBUG
endpoints:
  engine: "ipc:///tmp/test-engine.sock"
  compiler: "ipc:///tmp/test-compiler.sock"
  pub_sub: "ipc:///tmp/test-pubsub.sock"
llm:
  provider: openai
  model: gpt-4
  max_retries: 5
budget:
  max_tokens_per_task: 200000
)";
  ofs.close();

  YamlConfig loader;
  auto result = loader.Load(config_path.string());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->process_name, "test-engine");
  EXPECT_EQ(result->log_level, "DEBUG");
  EXPECT_EQ(result->endpoints.engine, "ipc:///tmp/test-engine.sock");
  EXPECT_EQ(result->llm.model, "gpt-4");
  EXPECT_EQ(result->llm.max_retries, 5);
  EXPECT_EQ(result->budget.max_tokens_per_task, 200000);

  std::filesystem::remove_all(test_dir);
}

TEST(YamlConfigTest, LoadNonexistentFile) {
  YamlConfig loader;
  auto result = loader.Load("/nonexistent/path.yaml");
  ASSERT_FALSE(result.has_value());
}
