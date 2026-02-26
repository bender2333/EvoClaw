#ifndef EVOCLAW_ENGINE_SAFETY_RISK_CLASSIFIER_H_
#define EVOCLAW_ENGINE_SAFETY_RISK_CLASSIFIER_H_

#include "protocol/message.h"
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace evoclaw {

/// RiskClassifier — 操作风险分级
/// L0: 只读操作
/// L1: 可逆写操作
/// L2: 不可逆写操作
/// L3: 外部交互（网络、邮件等）
class RiskClassifier {
 public:
  RiskClassifier();

  /// 对操作进行风险分级，返回 0-3
  int Classify(const Message& action) const;

  /// 注册自定义操作类型的风险等级
  void RegisterRisk(const std::string& action_type, int level);

 private:
  std::unordered_map<std::string, int> risk_map_;

  /// 从 payload 推断风险等级
  int InferFromPayload(const Message& action) const;
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_SAFETY_RISK_CLASSIFIER_H_
