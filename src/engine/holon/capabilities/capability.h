#ifndef EVOCLAW_ENGINE_HOLON_CAPABILITIES_CAPABILITY_H_
#define EVOCLAW_ENGINE_HOLON_CAPABILITIES_CAPABILITY_H_

#include "config/config.h"
#include <expected>
#include <nlohmann/json.hpp>
#include <string>

namespace evoclaw {

/// Capability — 系统能力纯虚接口
/// 每个能力封装一类系统操作（文件/Shell/网络等）
class Capability {
 public:
  virtual ~Capability() = default;

  /// 执行操作
  virtual std::expected<nlohmann::json, Error> Execute(
      const nlohmann::json& params) = 0;

  /// 能力名称
  virtual std::string Name() const = 0;

  /// 能力描述
  virtual std::string Description() const = 0;

  /// 风险等级 0-3
  virtual int RiskLevel() const = 0;
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_HOLON_CAPABILITIES_CAPABILITY_H_
