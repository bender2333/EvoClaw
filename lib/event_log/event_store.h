#ifndef EVOCLAW_EVENT_LOG_EVENT_STORE_H_
#define EVOCLAW_EVENT_LOG_EVENT_STORE_H_

#include "protocol/message.h"
#include "config/config.h"
#include <expected>
#include <functional>
#include <string>
#include <vector>

namespace evoclaw {

/// 事件存储纯虚接口（append-only）
class EventStore {
 public:
  virtual ~EventStore() = default;

  /// 追加事件（不可变，写入后只读）
  virtual std::expected<void, Error> Append(const Event& event) = 0;

  /// 按时间范围查询事件
  virtual std::expected<std::vector<Event>, Error> Query(
      const std::string& start_time,
      const std::string& end_time) = 0;

  /// 按事件类型查询
  virtual std::expected<std::vector<Event>, Error> QueryByType(
      const std::string& event_type,
      int limit = 100) = 0;

  /// 按事件 ID 查询
  virtual std::expected<Event, Error> GetById(const std::string& event_id) = 0;

  /// 遍历所有事件（用于 Compiler 模式识别）
  virtual std::expected<void, Error> ForEach(
      std::function<bool(const Event&)> callback) = 0;
};

}  // namespace evoclaw

#endif  // EVOCLAW_EVENT_LOG_EVENT_STORE_H_
