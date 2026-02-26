#include "safety/risk_classifier.h"

namespace evoclaw {

RiskClassifier::RiskClassifier() {
  // 默认风险分级
  risk_map_["file.read"] = 0;
  risk_map_["file.list"] = 0;
  risk_map_["file.stat"] = 0;
  risk_map_["memory.read"] = 0;
  risk_map_["config.read"] = 0;

  risk_map_["file.write"] = 1;
  risk_map_["file.create"] = 1;
  risk_map_["memory.write"] = 1;
  risk_map_["config.write"] = 1;

  risk_map_["file.delete"] = 2;
  risk_map_["shell.execute"] = 2;
  risk_map_["process.kill"] = 2;

  risk_map_["network.request"] = 3;
  risk_map_["email.send"] = 3;
  risk_map_["calendar.write"] = 3;
  risk_map_["publish.release"] = 3;
}

int RiskClassifier::Classify(const Message& action) const {
  // 先查精确匹配
  if (auto it = risk_map_.find(action.msg_type); it != risk_map_.end()) {
    return it->second;
  }

  // 再查前缀匹配（取第一段）
  auto dot = action.msg_type.find('.');
  if (dot != std::string::npos) {
    std::string prefix = action.msg_type.substr(0, dot);
    // 网络类操作默认 L3
    if (prefix == "network" || prefix == "email") return 3;
    // 删除类操作默认 L2
    if (prefix == "delete" || prefix == "shell" || prefix == "process") return 2;
    // 写类操作默认 L1
    if (prefix == "write" || prefix == "create") return 1;
  }

  // 从 payload 推断
  return InferFromPayload(action);
}

void RiskClassifier::RegisterRisk(const std::string& action_type, int level) {
  risk_map_[action_type] = level;
}

int RiskClassifier::InferFromPayload(const Message& action) const {
  // 检查 payload 中是否有风险标记
  if (action.payload.contains("risk_level")) {
    return action.payload["risk_level"].get<int>();
  }
  // 默认 L0（只读）
  return 0;
}

}  // namespace evoclaw
