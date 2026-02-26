#include "governance/entropy_monitor.h"

namespace evoclaw {

void EntropyMonitor::SetThreshold(const std::string& metric, double threshold) {
  std::lock_guard lock(mutex_);
  thresholds_[metric] = threshold;
}

void EntropyMonitor::Update(const std::string& metric, double value) {
  std::lock_guard lock(mutex_);
  values_[metric] = value;
}

void EntropyMonitor::UpdateAll(const std::unordered_map<std::string, double>& metrics) {
  std::lock_guard lock(mutex_);
  for (const auto& [k, v] : metrics) {
    values_[k] = v;
  }
}

std::vector<EntropyAlert> EntropyMonitor::Check() const {
  std::lock_guard lock(mutex_);
  std::vector<EntropyAlert> alerts;
  for (const auto& [metric, threshold] : thresholds_) {
    if (auto it = values_.find(metric); it != values_.end()) {
      if (it->second > threshold) {
        alerts.push_back({metric, it->second, threshold});
      }
    }
  }
  return alerts;
}

double EntropyMonitor::GetValue(const std::string& metric) const {
  std::lock_guard lock(mutex_);
  if (auto it = values_.find(metric); it != values_.end()) {
    return it->second;
  }
  return 0.0;
}

void EntropyMonitor::InitDefaults() {
  std::lock_guard lock(mutex_);
  // 5 个核心熵指标及默认阈值
  thresholds_["primitive_density"] = 0.8;      // 原语密度
  thresholds_["module_density"] = 0.7;         // 模块密度
  thresholds_["dependency_depth"] = 5.0;       // 依赖深度
  thresholds_["task_rounds"] = 15.0;           // 任务轮次
  thresholds_["exploration_backlog"] = 10.0;   // 探索区积压

  for (const auto& [k, _] : thresholds_) {
    values_[k] = 0.0;
  }
}

}  // namespace evoclaw
