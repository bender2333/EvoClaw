#ifndef EVOCLAW_PROTOCOL_RULE_ENGINE_H_
#define EVOCLAW_PROTOCOL_RULE_ENGINE_H_

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "common/error.h"

namespace evoclaw::protocol {

/// 约束验证结果
struct ValidationResult {
  bool passed = true;
  std::vector<std::string> violations;  // 违反的约束描述
};

/// RuleEngine 纯虚基类 — 形式约束验证接口
///
/// MVP 实现 BasicRuleEngine（数值/范围/布尔验证），
/// Phase 2 可替换为 Z3 求解器实现。
///
/// 架构文档：约束验证 ≤100ms（NFR3）
class RuleEngine {
 public:
  virtual ~RuleEngine() = default;

  /// 验证 payload 是否满足指定约束规则
  /// @param payload  待验证的 JSON 数据
  /// @param rules    约束规则定义（JSON 格式）
  /// @return 验证结果，包含是否通过和违反列表
  virtual Result<ValidationResult> Validate(
      const nlohmann::json& payload,
      const nlohmann::json& rules) const = 0;
};

}  // namespace evoclaw::protocol

#endif  // EVOCLAW_PROTOCOL_RULE_ENGINE_H_
