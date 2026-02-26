#ifndef EVOCLAW_PROTOCOL_RULE_ENGINE_H_
#define EVOCLAW_PROTOCOL_RULE_ENGINE_H_

namespace evoclaw::protocol {

/// RuleEngine 纯虚基类 — Story 1.3 定义接口
class RuleEngine {
 public:
  virtual ~RuleEngine() = default;
};

}  // namespace evoclaw::protocol

#endif  // EVOCLAW_PROTOCOL_RULE_ENGINE_H_
