#ifndef EVOCLAW_CONFIG_CONFIG_H_
#define EVOCLAW_CONFIG_CONFIG_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/error.h"

namespace evoclaw::config {

/// 配置接口 — 所有配置源的抽象基类
class Config {
 public:
  virtual ~Config() = default;

  /// 获取字符串值
  virtual Result<std::string> GetString(std::string_view key) const = 0;

  /// 获取整数值
  virtual Result<int> GetInt(std::string_view key) const = 0;

  /// 获取布尔值
  virtual Result<bool> GetBool(std::string_view key) const = 0;

  /// 获取字符串值，缺失时返回默认值
  virtual std::string GetStringOr(std::string_view key,
                                  std::string_view default_val) const = 0;

  /// 获取整数值，缺失时返回默认值
  virtual int GetIntOr(std::string_view key, int default_val) const = 0;

  /// 获取布尔值，缺失时返回默认值
  virtual bool GetBoolOr(std::string_view key, bool default_val) const = 0;
};

}  // namespace evoclaw::config

#endif  // EVOCLAW_CONFIG_CONFIG_H_
