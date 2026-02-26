#include "governance/governance_gate.h"

namespace evoclaw {

std::expected<bool, Error> GovernanceGate::Approve(const Message& action, int risk_level) {
  std::lock_guard lock(mutex_);

  // L0/L1: 自动通过
  if (risk_level <= 1) {
    status_[action.msg_id] = ApprovalStatus::kApproved;
    audit_log_.push_back(action.msg_id);
    return true;
  }

  // L2: 记录日志后通过
  if (risk_level == 2) {
    status_[action.msg_id] = ApprovalStatus::kApproved;
    audit_log_.push_back(action.msg_id);
    return true;
  }

  // L3/L4: 需要用户确认
  pending_[action.msg_id] = action;
  status_[action.msg_id] = ApprovalStatus::kPending;
  audit_log_.push_back(action.msg_id);
  return false;
}

std::vector<Message> GovernanceGate::GetPendingApprovals() const {
  std::lock_guard lock(mutex_);
  std::vector<Message> result;
  result.reserve(pending_.size());
  for (const auto& [id, msg] : pending_) {
    result.push_back(msg);
  }
  return result;
}

std::expected<void, Error> GovernanceGate::UserApprove(const std::string& msg_id) {
  std::lock_guard lock(mutex_);
  auto it = pending_.find(msg_id);
  if (it == pending_.end()) {
    return std::unexpected(Error{Error::Code::kNotFound,
        "No pending approval for: " + msg_id, "GovernanceGate::UserApprove"});
  }
  status_[msg_id] = ApprovalStatus::kApproved;
  pending_.erase(it);
  return {};
}

std::expected<void, Error> GovernanceGate::UserReject(const std::string& msg_id) {
  std::lock_guard lock(mutex_);
  auto it = pending_.find(msg_id);
  if (it == pending_.end()) {
    return std::unexpected(Error{Error::Code::kNotFound,
        "No pending approval for: " + msg_id, "GovernanceGate::UserReject"});
  }
  status_[msg_id] = ApprovalStatus::kRejected;
  pending_.erase(it);
  return {};
}

std::expected<ApprovalStatus, Error> GovernanceGate::GetStatus(const std::string& msg_id) const {
  std::lock_guard lock(mutex_);
  auto it = status_.find(msg_id);
  if (it == status_.end()) {
    return std::unexpected(Error{Error::Code::kNotFound,
        "Unknown msg_id: " + msg_id, "GovernanceGate::GetStatus"});
  }
  return it->second;
}

size_t GovernanceGate::GetAuditLogSize() const {
  std::lock_guard lock(mutex_);
  return audit_log_.size();
}

}  // namespace evoclaw
