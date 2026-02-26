#ifndef EVOCLAW_ENGINE_HOLON_CAPABILITIES_SHELL_CAPABILITY_H_
#define EVOCLAW_ENGINE_HOLON_CAPABILITIES_SHELL_CAPABILITY_H_

#include "holon/capabilities/capability.h"
#include <unordered_set>

namespace evoclaw {

/// ShellCapability — Shell 命令执行能力
/// 内置命令黑名单，禁止危险操作
class ShellCapability : public Capability {
 public:
  ShellCapability();

  std::expected<nlohmann::json, Error> Execute(
      const nlohmann::json& params) override;
  std::string Name() const override { return "shell"; }
  std::string Description() const override { return "Shell 命令执行"; }
  int RiskLevel() const override { return 2; }

 private:
  std::unordered_set<std::string> blacklist_;

  /// 检查命令是否包含黑名单关键词
  bool IsDangerous(const std::string& command) const;
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_HOLON_CAPABILITIES_SHELL_CAPABILITY_H_
