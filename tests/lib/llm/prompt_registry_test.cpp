#include <gtest/gtest.h>
#include "llm/prompt_registry.h"
#include "common/uuid.h"
#include <filesystem>
#include <fstream>

using namespace evoclaw;

class PromptRegistryTest : public ::testing::Test {
 protected:
  std::filesystem::path test_dir_;

  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_prompt_" + GenerateUuid());
    std::filesystem::create_directories(test_dir_ / "facade");

    // 创建测试 prompt
    {
      std::ofstream ofs(test_dir_ / "facade" / "system_persona.yaml");
      ofs << "id: facade.system_persona\nversion: 1\noutput_schema: text\n";
    }
    {
      std::ofstream ofs(test_dir_ / "facade" / "system_persona.txt");
      ofs << "你是 {{name}}，一个智能管家。当前时间：{{time}}。";
    }
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }
};

TEST_F(PromptRegistryTest, LoadAndGet) {
  PromptRegistry registry;
  auto load_result = registry.LoadFromDir(test_dir_);
  ASSERT_TRUE(load_result.has_value());

  auto get_result = registry.Get("facade.system_persona");
  ASSERT_TRUE(get_result.has_value());
  EXPECT_EQ(get_result->id, "facade.system_persona");
  EXPECT_EQ(get_result->version, 1);
  EXPECT_FALSE(get_result->template_text.empty());
}

TEST_F(PromptRegistryTest, Render) {
  PromptRegistry registry;
  registry.LoadFromDir(test_dir_);

  auto result = registry.Render("facade.system_persona", {
      {"name", "EvoClaw"},
      {"time", "2026-02-26"},
  });
  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->find("EvoClaw"), std::string::npos);
  EXPECT_NE(result->find("2026-02-26"), std::string::npos);
}

TEST_F(PromptRegistryTest, GetNotFound) {
  PromptRegistry registry;
  registry.LoadFromDir(test_dir_);

  auto result = registry.Get("nonexistent.prompt");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, Error::Code::kNotFound);
}

TEST_F(PromptRegistryTest, ListIds) {
  PromptRegistry registry;
  registry.LoadFromDir(test_dir_);

  auto ids = registry.ListIds();
  EXPECT_EQ(ids.size(), 1u);
}
