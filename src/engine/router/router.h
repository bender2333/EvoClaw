#ifndef EVOCLAW_ENGINE_ROUTER_ROUTER_H_
#define EVOCLAW_ENGINE_ROUTER_ROUTER_H_

#include "config/config.h"
#include "protocol/message.h"
#include "router/capability_matrix.h"
#include <expected>
#include <random>
#include <string>

namespace evoclaw {

/// Fast Router — ε-greedy 路由策略
/// 以概率 (1-ε) 选择评分最高的模块，以概率 ε 随机探索
/// 不执行任务，不修改输出，不理解模块内部实现
class Router {
 public:
  /// @param matrix 能力矩阵引用
  /// @param epsilon 探索概率，默认 0.1
  explicit Router(CapabilityMatrix& matrix, double epsilon = 0.1);

  /// 路由决策：根据消息的 msg_type 选择最佳模块
  /// @return 选中的 module_id
  std::expected<std::string, Error> Route(const Message& msg);

  /// 获取当前 epsilon 值
  double GetEpsilon() const { return epsilon_; }

  /// 设置 epsilon 值
  void SetEpsilon(double epsilon) { epsilon_ = epsilon; }

 private:
  CapabilityMatrix& matrix_;
  double epsilon_;
  std::mt19937 rng_;

  /// 从 msg_type 提取能力名称（取第一段，如 "task.request" -> "task"）
  static std::string ExtractCapability(const std::string& msg_type);
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_ROUTER_ROUTER_H_
