#ifndef EVOCLAW_CONFIG_CONFIG_H_
#define EVOCLAW_CONFIG_CONFIG_H_

#include <string>

#include "common/error.h"

namespace evoclaw::config {

/// 配置接口 — Story 1.2 实现
class Config {
 public:
  virtual ~Config() = default;
};

}  // namespace evoclaw::config

#endif  // EVOCLAW_CONFIG_CONFIG_H_
