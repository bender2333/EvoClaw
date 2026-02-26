#ifndef EVOCLAW_PROTOCOL_BASIC_RULE_ENGINE_H_
#define EVOCLAW_PROTOCOL_BASIC_RULE_ENGINE_H_

#include "protocol/rule_engine.h"

namespace evoclaw::protocol {

/// 基础规则引擎 — Story 1.3 接口定义，Story 7.2 完整实现
///
/// 支持的约束类型（MVP）：
///   - "required": 字段必须存在
///   - "type": 字段类型检查（string/number/boolean/array/object）
///   - "min" / "max": 数值范围
///   - "min_length" / "max_length": 字符串长度
///   - "enum": 枚举值白名单
///
/// 规则格式示例：
/// {
///   "field_name": {
///     "required": true,
///     "type": "number",
///     "min": 0,
///     "max": 100
///   }
/// }
class BasicRuleEngine : public RuleEngine {
 public:
  Result<ValidationResult> Validate(
      const nlohmann::json& payload,
      const nlohmann::json& rules) const override;

 private:
  /// 验证单个字段
  void ValidateField(const std::string& field_name,
                     const nlohmann::json& payload,
                     const nlohmann::json& field_rules,
                     ValidationResult& result) const;
};

}  // namespace evoclaw::protocol

#endif  // EVOCLAW_PROTOCOL_BASIC_RULE_ENGINE_H_
