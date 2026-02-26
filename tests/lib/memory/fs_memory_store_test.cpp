#include <gtest/gtest.h>
#include "memory/fs_memory_store.h"
#include "common/uuid.h"
#include <filesystem>

using namespace evoclaw;

class FsMemoryStoreTest : public ::testing::Test {
 protected:
  std::filesystem::path test_dir_;
  std::unique_ptr<FsMemoryStore> store_;

  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_mem_" + GenerateUuid());
    store_ = std::make_unique<FsMemoryStore>(test_dir_);
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }
};

TEST_F(FsMemoryStoreTest, StoreAndGet) {
  MemoryEntry entry;
  entry.id = GenerateUuid();
  entry.type = MemoryType::kWorking;
  entry.timestamp = "2026-02-26T10:30:00Z";
  entry.summary = "Test working memory";
  entry.content = {{"task", "research"}};

  auto store_result = store_->Store(entry);
  ASSERT_TRUE(store_result.has_value());

  auto get_result = store_->Get(entry.id);
  ASSERT_TRUE(get_result.has_value());
  EXPECT_EQ(get_result->id, entry.id);
  EXPECT_EQ(get_result->summary, "Test working memory");
}

TEST_F(FsMemoryStoreTest, QueryByType) {
  for (int i = 0; i < 3; ++i) {
    MemoryEntry entry;
    entry.id = GenerateUuid();
    entry.type = MemoryType::kEpisodic;
    entry.timestamp = "2026-02-26T10:30:00Z";
    entry.summary = "Episode " + std::to_string(i);
    store_->Store(entry);
  }

  auto result = store_->QueryByType(MemoryType::kEpisodic, 10);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 3u);
}

TEST_F(FsMemoryStoreTest, Search) {
  MemoryEntry entry;
  entry.id = GenerateUuid();
  entry.type = MemoryType::kSemantic;
  entry.timestamp = "2026-02-26T10:30:00Z";
  entry.summary = "Agent collaboration pattern for research tasks";
  store_->Store(entry);

  auto result = store_->Search("collaboration", 10);
  ASSERT_TRUE(result.has_value());
  EXPECT_GE(result->size(), 1u);
}

TEST_F(FsMemoryStoreTest, Delete) {
  MemoryEntry entry;
  entry.id = GenerateUuid();
  entry.type = MemoryType::kWorking;
  entry.timestamp = "2026-02-26T10:30:00Z";
  entry.summary = "To be deleted";
  store_->Store(entry);

  auto del_result = store_->Delete(entry.id);
  ASSERT_TRUE(del_result.has_value());

  auto get_result = store_->Get(entry.id);
  ASSERT_FALSE(get_result.has_value());
}
