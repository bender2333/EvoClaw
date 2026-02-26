#ifndef EVOCLAW_PROTOCOL_RULE_ENGINE_H_
#define EVOCLAW_PROTOCOL_RULE_ENGINE_H_

#include "config/config.h"
#include <expected>
#include <string>
#include <nlohmann/json.hpp>

namespace evoclaw {

/// 形式约束验证结果
struct ValidationResult {
  bool passed = true;
  std::string reason;
};

/// 形式约束验证接口（Cortex Layer III）
/// MVP 用规则引擎实现，Phase 2 可替换为 Z3
class RuleEngine {
 public:
  virtual ~RuleEngine() = default;

  /// 验证 payload 是否满足形式约束
  virtual std::expected<ValidationResult, Error> Validate(
      const nlohmann::json& payload,
      const nlohmann::json& constraints) = 0;
};

}  // namespace evoclaw

#endif  // EVOCLAW_PROTOCOL_RULE_ENGINE_H_
