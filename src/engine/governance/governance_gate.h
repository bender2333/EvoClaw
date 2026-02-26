#ifndef EVOCLAW_ENGINE_GOVERNANCE_GOVERNANCE_GATE_H_
#define EVOCLAW_ENGINE_GOVERNANCE_GOVERNANCE_GATE_H_

#include "config/config.h"
#include "protocol/message.h"
#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace evoclaw {

/// 审批状态
enum class ApprovalStatus { kPending, kApproved, kRejected };

/// GovernanceGate — 治理门控
/// 所有操作必须经过审批。根据 risk_level 决定自动通过或需要用户确认。
/// L0/L1 自动通过，L2 记录日志后通过，L3/L4 需要用户确认。
class GovernanceGate {
 public:
  /// 审批操作。返回 true 表示通过，false 表示需要等待用户确认。
  std::expected<bool, Error> Approve(const Message& action, int risk_level);

  /// 获取所有待审批操作
  std::vector<Message> GetPendingApprovals() const;

  /// 用户批准操作
  std::expected<void, Error> UserApprove(const std::string& msg_id);

  /// 用户拒绝操作
  std::expected<void, Error> UserReject(const std::string& msg_id);

  /// 查询操作审批状态
  std::expected<ApprovalStatus, Error> GetStatus(const std::string& msg_id) const;

  /// 获取审批日志条数
  size_t GetAuditLogSize() const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Message> pending_;
  std::unordered_map<std::string, ApprovalStatus> status_;
  std::vector<std::string> audit_log_;  // msg_id 审计记录
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_GOVERNANCE_GOVERNANCE_GATE_H_
