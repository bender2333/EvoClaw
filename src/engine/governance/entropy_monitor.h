#ifndef EVOCLAW_ENGINE_GOVERNANCE_ENTROPY_MONITOR_H_
#define EVOCLAW_ENGINE_GOVERNANCE_ENTROPY_MONITOR_H_

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace evoclaw {

/// 熵告警
struct EntropyAlert {
  std::string metric_name;
  double current_value;
  double threshold;
};

/// EntropyMonitor — 熵监控器
/// 监控 5 个熵指标，超阈值触发反熵机制
class EntropyMonitor {
 public:
  /// 设置指标阈值
  void SetThreshold(const std::string& metric, double threshold);

  /// 更新指标值
  void Update(const std::string& metric, double value);

  /// 批量更新
  void UpdateAll(const std::unordered_map<std::string, double>& metrics);

  /// 检查所有指标，返回超阈值的告警列表
  std::vector<EntropyAlert> Check() const;

  /// 获取指标当前值
  double GetValue(const std::string& metric) const;

  /// 初始化默认阈值（5 个核心熵指标）
  void InitDefaults();

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, double> values_;
  std::unordered_map<std::string, double> thresholds_;
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_GOVERNANCE_ENTROPY_MONITOR_H_
