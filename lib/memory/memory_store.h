#ifndef EVOCLAW_MEMORY_MEMORY_STORE_H_
#define EVOCLAW_MEMORY_MEMORY_STORE_H_

#include "common/error.h"

namespace evoclaw::memory {

/// MemoryStore 纯虚基类 — Story 2.2 实现
class MemoryStore {
 public:
  virtual ~MemoryStore() = default;
};

}  // namespace evoclaw::memory

#endif  // EVOCLAW_MEMORY_MEMORY_STORE_H_
