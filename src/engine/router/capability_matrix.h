#ifndef EVOCLAW_ENGINE_ROUTER_CAPABILITY_MATRIX_H_
#define EVOCLAW_ENGINE_ROUTER_CAPABILITY_MATRIX_H_

#include "config/config.h"
#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace evoclaw {

/// 模块能力评分记录
struct ModuleScore {
  std::string module_id;
  double score;
};

/// 能力矩阵 — 维护所有模块在各能力维度上的历史质量评分
/// 线程安全，支持并发读写
class CapabilityMatrix {
 public:
  /// 注册模块及其初始能力评分
  void RegisterModule(const std::string& module_id,
                      const std::unordered_map<std::string, double>& capabilities);

  /// 更新单个模块在某能力上的评分
  void UpdateScore(const std::string& module_id,
                   const std::string& capability, double score);

  /// 获取某能力维度上评分最高的模块
  std::expected<ModuleScore, Error> GetBestModule(const std::string& capability) const;

  /// 获取某能力维度上所有模块的评分（降序）
  std::vector<ModuleScore> GetAllScores(const std::string& capability) const;

  /// 获取某能力维度上所有已注册模块 ID
  std::vector<std::string> GetModuleIds(const std::string& capability) const;

  /// 模块是否已注册
  bool HasModule(const std::string& module_id) const;

  /// 移除模块
  void RemoveModule(const std::string& module_id);

 private:
  mutable std::mutex mutex_;
  // module_id -> { capability -> score }
  std::unordered_map<std::string, std::unordered_map<std::string, double>> matrix_;
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_ROUTER_CAPABILITY_MATRIX_H_
