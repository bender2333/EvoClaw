#ifndef EVOCLAW_ENGINE_HOLON_CAPABILITIES_FILE_CAPABILITY_H_
#define EVOCLAW_ENGINE_HOLON_CAPABILITIES_FILE_CAPABILITY_H_

#include "holon/capabilities/capability.h"

namespace evoclaw {

/// FileCapability — 文件操作能力
/// 支持 read/write/list/stat 操作
class FileCapability : public Capability {
 public:
  std::expected<nlohmann::json, Error> Execute(
      const nlohmann::json& params) override;
  std::string Name() const override { return "file"; }
  std::string Description() const override { return "文件读写操作"; }
  int RiskLevel() const override { return 1; }
};

}  // namespace evoclaw

#endif  // EVOCLAW_ENGINE_HOLON_CAPABILITIES_FILE_CAPABILITY_H_
