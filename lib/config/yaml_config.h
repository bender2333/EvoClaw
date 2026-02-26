#ifndef EVOCLAW_CONFIG_YAML_CONFIG_H_
#define EVOCLAW_CONFIG_YAML_CONFIG_H_

#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include "config/config.h"

namespace evoclaw::config {

/// YAML 配置加载 — Story 1.2 实现
///
/// 支持点分路径访问（如 "bus.engine_pub"）和环境变量覆盖。
/// 环境变量规则：EVOCLAW_ 前缀 + 大写 + 下划线替换点号
///   例：bus.engine_pub → EVOCLAW_BUS_ENGINE_PUB
class YamlConfig : public Config {
 public:
  /// 从文件加载配置
  static Result<YamlConfig> Load(std::string_view path);

  // ── Config 接口实现 ──────────────────────────────────
  Result<std::string> GetString(std::string_view key) const override;
  Result<int> GetInt(std::string_view key) const override;
  Result<bool> GetBool(std::string_view key) const override;
  std::string GetStringOr(std::string_view key,
                          std::string_view default_val) const override;
  int GetIntOr(std::string_view key, int default_val) const override;
  bool GetBoolOr(std::string_view key, bool default_val) const override;

 private:
  explicit YamlConfig(YAML::Node root);

  /// 按点分路径查找 YAML 节点
  YAML::Node Resolve(std::string_view dotted_key) const;

  /// 检查环境变量覆盖
  static std::optional<std::string> EnvOverride(std::string_view key);

  /// 将点分 key 转为环境变量名：bus.engine_pub → EVOCLAW_BUS_ENGINE_PUB
  static std::string KeyToEnvName(std::string_view key);

  YAML::Node root_;
};

}  // namespace evoclaw::config

#endif  // EVOCLAW_CONFIG_YAML_CONFIG_H_
