#ifndef EVOCLAW_CONFIG_YAML_CONFIG_H_
#define EVOCLAW_CONFIG_YAML_CONFIG_H_

#include "config/config.h"

namespace evoclaw {

/// YAML 配置加载实现
class YamlConfig : public Config {
 public:
  std::expected<AppConfig, Error> Load(const std::string& path) override;
};

}  // namespace evoclaw

#endif  // EVOCLAW_CONFIG_YAML_CONFIG_H_
