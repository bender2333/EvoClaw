#ifndef EVOCLAW_EVENT_LOG_EVENT_STORE_H_
#define EVOCLAW_EVENT_LOG_EVENT_STORE_H_

#include "common/error.h"

namespace evoclaw::event_log {

/// EventStore 纯虚基类 — Story 1.5 实现
class EventStore {
 public:
  virtual ~EventStore() = default;
};

}  // namespace evoclaw::event_log

#endif  // EVOCLAW_EVENT_LOG_EVENT_STORE_H_
