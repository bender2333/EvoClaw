#ifndef EVOCLAW_PROTOCOL_BASIC_RULE_ENGINE_H_
#define EVOCLAW_PROTOCOL_BASIC_RULE_ENGINE_H_

#include "protocol/rule_engine.h"

namespace evoclaw {

/// MVP 规则引擎：数值比较/布尔逻辑/范围检查
/// Phase 2 可替换为 Z3 FormalVerifier
class BasicRuleEngine : public RuleEngine {
 public:
  std::expected<ValidationResult, Error> Validate(
      const nlohmann::json& payload,
      const nlohmann::json& constraints) override;
};

}  // namespace evoclaw

#endif  // EVOCLAW_PROTOCOL_BASIC_RULE_ENGINE_H_
