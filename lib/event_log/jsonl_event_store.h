#ifndef EVOCLAW_EVENT_LOG_JSONL_EVENT_STORE_H_
#define EVOCLAW_EVENT_LOG_JSONL_EVENT_STORE_H_

#include "event_log/event_store.h"
#include <filesystem>
#include <mutex>

namespace evoclaw {

/// JSON Lines 事件存储实现
/// 每天一个文件（data/events/YYYY-MM-DD.jsonl），写入后只读 + SHA256 校验
class JsonlEventStore : public EventStore {
 public:
  explicit JsonlEventStore(const std::filesystem::path& data_dir);

  std::expected<void, Error> Append(const Event& event) override;
  std::expected<std::vector<Event>, Error> Query(
      const std::string& start_time, const std::string& end_time) override;
  std::expected<std::vector<Event>, Error> QueryByType(
      const std::string& event_type, int limit) override;
  std::expected<Event, Error> GetById(const std::string& event_id) override;
  std::expected<void, Error> ForEach(
      std::function<bool(const Event&)> callback) override;

 private:
  std::filesystem::path data_dir_;
  std::mutex write_mutex_;

  std::filesystem::path GetFilePath(const std::string& date) const;
  std::string TodayDate() const;
  std::expected<std::vector<Event>, Error> ReadFile(const std::filesystem::path& path) const;
};

}  // namespace evoclaw

#endif  // EVOCLAW_EVENT_LOG_JSONL_EVENT_STORE_H_
