#include "safety/permission_manager.h"

namespace evoclaw {

void PermissionManager::GrantPermission(const std::string& agent_id,
                                         const std::string& action_type) {
  std::lock_guard lock(mutex_);
  permissions_[agent_id].insert(action_type);
}

void PermissionManager::RevokePermission(const std::string& agent_id,
                                          const std::string& action_type) {
  std::lock_guard lock(mutex_);
  if (auto it = permissions_.find(agent_id); it != permissions_.end()) {
    it->second.erase(action_type);
  }
}

bool PermissionManager::HasPermission(const std::string& agent_id,
                                       const std::string& action_type) const {
  std::lock_guard lock(mutex_);
  if (super_agents_.contains(agent_id)) return true;
  if (auto it = permissions_.find(agent_id); it != permissions_.end()) {
    return it->second.contains(action_type);
  }
  return false;
}

std::unordered_set<std::string> PermissionManager::GetPermissions(
    const std::string& agent_id) const {
  std::lock_guard lock(mutex_);
  if (auto it = permissions_.find(agent_id); it != permissions_.end()) {
    return it->second;
  }
  return {};
}

void PermissionManager::GrantAll(const std::string& agent_id) {
  std::lock_guard lock(mutex_);
  super_agents_.insert(agent_id);
}

bool PermissionManager::HasAllPermissions(const std::string& agent_id) const {
  std::lock_guard lock(mutex_);
  return super_agents_.contains(agent_id);
}

}  // namespace evoclaw
