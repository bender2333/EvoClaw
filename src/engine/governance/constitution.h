#ifndef EVOCLAW_ENGINE_GOVERNANCE_CONSTITUTION_H_
#define EVOCLAW_ENGINE_GOVERNANCE_CONSTITUTION_H_

#include "config/config.h"
#include "protocol/message.h"
#include <chrono>
#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace evoclaw {

/// 宪法规则
struct ConstitutionRule {
  std::string id;
  std::string description;
  std::string forbidden_action;  // 禁止的操作类型（msg_type 前缀匹配）
};

/// 宪法修正案
struct Amendment {
  std::string id;
  ConstitutionRule rule;
  std::chrono::system_clock::time_point proposed_at;
  bool confirmed = false;
  static constexpr auto kCooldownPeriod = std::chrono::hours(24);
};

/// Constitution — 宪法
/// 定义所有角色不能跨越的行为边界。
/// 宪法不可自动修改，只能用户授权 + ≥24h 冷静期。
class Constitution {
 public:
  /// 从 YAML 文件加载宪法规则
  std::expected<void, Error> LoadFromFile(const std::string& path);

  /// 手动添加规则
  void AddRule(const ConstitutionRule& rule);

  /// 检查操作是否违反宪法。返回 true 表示合规，false 表示违反。
  std::expected<bool, Error> Check(const Message& action) const;

  /// 提出宪法修正案（开始冷静期）
  std::expected<std::string, Error> ProposeAmendment(const ConstitutionRule& rule);

  /// 确认修正案（检查冷静期是否已过）
  std::expected<bool, Error> ConfirmAmendment(const std::string& amendment_id);

  /// 获取当前规则数量
  size_t GetRuleCount() const;

 private:
  mutable std::mutex mutex_;
  std::vector<ConstitutionRule> rules_;
  std::unordered_map<std::string, Amendment> amendments_;
  int next_amendment_id_ = 1;
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_GOVERNANCE_CONSTITUTION_H_
