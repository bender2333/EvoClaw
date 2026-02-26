#ifndef EVOCLAW_BUS_MESSAGE_BUS_H_
#define EVOCLAW_BUS_MESSAGE_BUS_H_

#include "common/error.h"

namespace evoclaw::bus {

/// MessageBus 纯虚基类 — Story 1.4 实现
class MessageBus {
 public:
  virtual ~MessageBus() = default;
};

}  // namespace evoclaw::bus

#endif  // EVOCLAW_BUS_MESSAGE_BUS_H_
