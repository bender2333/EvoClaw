#include "router/router.h"
#include <algorithm>

namespace evoclaw {

Router::Router(CapabilityMatrix& matrix, double epsilon)
    : matrix_(matrix),
      epsilon_(epsilon),
      rng_(std::random_device{}()) {}

std::expected<std::string, Error> Router::Route(const Message& msg) {
  std::string capability = ExtractCapability(msg.msg_type);
  if (capability.empty()) {
    return std::unexpected(Error{
        Error::Code::kInvalidArg,
        "Empty msg_type, cannot route",
        "Router::Route"});
  }

  auto all = matrix_.GetAllScores(capability);
  if (all.empty()) {
    return std::unexpected(Error{
        Error::Code::kNotFound,
        "No module registered for capability: " + capability,
        "Router::Route"});
  }

  // ε-greedy：以概率 ε 随机探索
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  if (dist(rng_) < epsilon_ && all.size() > 1) {
    // 随机选择（排除最佳）
    std::uniform_int_distribution<size_t> idx_dist(1, all.size() - 1);
    return all[idx_dist(rng_)].module_id;
  }

  // 以概率 (1-ε) 选择评分最高的模块
  return all[0].module_id;
}

std::string Router::ExtractCapability(const std::string& msg_type) {
  auto dot = msg_type.find('.');
  if (dot == std::string::npos) return msg_type;
  return msg_type.substr(0, dot);
}

}  // namespace evoclaw
