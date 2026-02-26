#include <gtest/gtest.h>
#include "event_log/jsonl_event_store.h"
#include "common/uuid.h"
#include <filesystem>

using namespace evoclaw;

class JsonlEventStoreTest : public ::testing::Test {
 protected:
  std::filesystem::path test_dir_;
  std::unique_ptr<JsonlEventStore> store_;

  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_test_" + GenerateUuid());
    store_ = std::make_unique<JsonlEventStore>(test_dir_);
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  Event MakeEvent(const std::string& type) {
    Event e;
    e.event_id = GenerateUuid();
    e.event_type = type;
    e.source_process = "engine";
    e.timestamp = "2026-02-26T10:30:00Z";
    e.data = {{"key", "value"}};
    return e;
  }
};

TEST_F(JsonlEventStoreTest, AppendAndGetById) {
  auto event = MakeEvent("agent.task.completed");
  auto append_result = store_->Append(event);
  ASSERT_TRUE(append_result.has_value());

  auto get_result = store_->GetById(event.event_id);
  ASSERT_TRUE(get_result.has_value());
  EXPECT_EQ(get_result->event_id, event.event_id);
  EXPECT_EQ(get_result->event_type, "agent.task.completed");
}

TEST_F(JsonlEventStoreTest, QueryByType) {
  store_->Append(MakeEvent("agent.task.completed"));
  store_->Append(MakeEvent("agent.task.completed"));
  store_->Append(MakeEvent("evolution.compile.started"));

  auto result = store_->QueryByType("agent.task.completed", 10);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2u);
}

TEST_F(JsonlEventStoreTest, ForEach) {
  store_->Append(MakeEvent("a"));
  store_->Append(MakeEvent("b"));
  store_->Append(MakeEvent("c"));

  int count = 0;
  store_->ForEach([&count](const Event&) { ++count; return true; });
  EXPECT_EQ(count, 3);
}

TEST_F(JsonlEventStoreTest, GetByIdNotFound) {
  auto result = store_->GetById("nonexistent");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, Error::Code::kNotFound);
}
