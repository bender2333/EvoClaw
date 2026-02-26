#include <gtest/gtest.h>
#include "event_log/event_merger.h"
#include "event_log/jsonl_event_store.h"
#include "common/uuid.h"
#include <filesystem>

using namespace evoclaw;

TEST(EventMergerTest, MergeRange) {
  auto test_dir = std::filesystem::temp_directory_path() / ("evoclaw_merge_" + GenerateUuid());
  JsonlEventStore store(test_dir);

  Event e1;
  e1.event_id = GenerateUuid();
  e1.event_type = "test.event";
  e1.source_process = "engine";
  e1.timestamp = "2026-02-26T10:00:00Z";
  e1.data = {{"seq", 1}};
  store.Append(e1);

  Event e2;
  e2.event_id = GenerateUuid();
  e2.event_type = "test.event";
  e2.source_process = "compiler";
  e2.timestamp = "2026-02-26T11:00:00Z";
  e2.data = {{"seq", 2}};
  store.Append(e2);

  EventMerger merger(store);
  auto output_path = test_dir / "merged.jsonl";
  auto result = merger.MergeRange("2026-02-26", "2026-02-26", output_path);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(std::filesystem::exists(output_path));

  std::filesystem::remove_all(test_dir);
}
