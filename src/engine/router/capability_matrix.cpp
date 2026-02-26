#include "router/capability_matrix.h"
#include <algorithm>

namespace evoclaw {

void CapabilityMatrix::RegisterModule(
    const std::string& module_id,
    const std::unordered_map<std::string, double>& capabilities) {
  std::lock_guard lock(mutex_);
  matrix_[module_id] = capabilities;
}

void CapabilityMatrix::UpdateScore(
    const std::string& module_id,
    const std::string& capability, double score) {
  std::lock_guard lock(mutex_);
  matrix_[module_id][capability] = score;
}

std::expected<ModuleScore, Error> CapabilityMatrix::GetBestModule(
    const std::string& capability) const {
  std::lock_guard lock(mutex_);
  ModuleScore best{"", -1.0};
  for (const auto& [mod_id, caps] : matrix_) {
    if (auto it = caps.find(capability); it != caps.end()) {
      if (it->second > best.score) {
        best = {mod_id, it->second};
      }
    }
  }
  if (best.module_id.empty()) {
    return std::unexpected(Error{
        Error::Code::kNotFound,
        "No module found for capability: " + capability,
        "CapabilityMatrix::GetBestModule"});
  }
  return best;
}

std::vector<ModuleScore> CapabilityMatrix::GetAllScores(
    const std::string& capability) const {
  std::lock_guard lock(mutex_);
  std::vector<ModuleScore> result;
  for (const auto& [mod_id, caps] : matrix_) {
    if (auto it = caps.find(capability); it != caps.end()) {
      result.push_back({mod_id, it->second});
    }
  }
  // 降序排列
  std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) { return a.score > b.score; });
  return result;
}

std::vector<std::string> CapabilityMatrix::GetModuleIds(
    const std::string& capability) const {
  std::lock_guard lock(mutex_);
  std::vector<std::string> ids;
  for (const auto& [mod_id, caps] : matrix_) {
    if (caps.contains(capability)) {
      ids.push_back(mod_id);
    }
  }
  return ids;
}

bool CapabilityMatrix::HasModule(const std::string& module_id) const {
  std::lock_guard lock(mutex_);
  return matrix_.contains(module_id);
}

void CapabilityMatrix::RemoveModule(const std::string& module_id) {
  std::lock_guard lock(mutex_);
  matrix_.erase(module_id);
}

}  // namespace evoclaw
