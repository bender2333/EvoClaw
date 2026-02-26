#ifndef EVOCLAW_ENGINE_SAFETY_PERMISSION_MANAGER_H_
#define EVOCLAW_ENGINE_SAFETY_PERMISSION_MANAGER_H_

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace evoclaw {

/// PermissionManager — 权限管理器
/// 管理 agent 对操作类型的权限
class PermissionManager {
 public:
  /// 授予权限
  void GrantPermission(const std::string& agent_id, const std::string& action_type);

  /// 撤销权限
  void RevokePermission(const std::string& agent_id, const std::string& action_type);

  /// 检查权限
  bool HasPermission(const std::string& agent_id, const std::string& action_type) const;

  /// 获取 agent 的所有权限
  std::unordered_set<std::string> GetPermissions(const std::string& agent_id) const;

  /// 授予 agent 所有权限（超级权限）
  void GrantAll(const std::string& agent_id);

  /// 是否拥有超级权限
  bool HasAllPermissions(const std::string& agent_id) const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::unordered_set<std::string>> permissions_;
  std::unordered_set<std::string> super_agents_;  // 拥有所有权限的 agent
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_SAFETY_PERMISSION_MANAGER_H_
