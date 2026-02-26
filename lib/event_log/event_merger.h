#ifndef EVOCLAW_EVENT_LOG_EVENT_MERGER_H_
#define EVOCLAW_EVENT_LOG_EVENT_MERGER_H_

#include "event_log/event_store.h"
#include <filesystem>

namespace evoclaw {

/// 事件日志合并器（用于归档和压缩）
class EventMerger {
 public:
  explicit EventMerger(EventStore& store) : store_(store) {}

  /// 合并指定日期范围的事件到单个归档文件
  std::expected<void, Error> MergeRange(
      const std::string& start_date,
      const std::string& end_date,
      const std::filesystem::path& output_path);

 private:
  EventStore& store_;
};

}  // namespace evoclaw

#endif  // EVOCLAW_EVENT_LOG_EVENT_MERGER_H_
