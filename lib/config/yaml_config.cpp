#include "config/yaml_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace evoclaw::config {

YamlConfig::YamlConfig(YAML::Node root) : root_(std::move(root)) {}

Result<YamlConfig> YamlConfig::Load(std::string_view path) {
  namespace fs = std::filesystem;

  if (!fs::exists(path)) {
    return std::unexpected(
        Error::Make(ErrorCode::kNotFound,
                    std::string("config file not found: ") + std::string(path),
                    "YamlConfig::Load"));
  }

  try {
    YAML::Node root = YAML::LoadFile(std::string(path));
    return YamlConfig(std::move(root));
  } catch (const YAML::Exception& e) {
    return std::unexpected(
        Error::Make(ErrorCode::kSerializationError,
                    std::string("YAML parse error: ") + e.what(),
                    "YamlConfig::Load"));
  }
}

// ── 接口实现 ──────────────────────────────────────────────

Result<std::string> YamlConfig::GetString(std::string_view key) const {
  // 环境变量优先
  if (auto env = EnvOverride(key)) {
    return *env;
  }

  auto node = Resolve(key);
  if (!node || node.IsNull()) {
    return std::unexpected(
        Error::Make(ErrorCode::kNotFound,
                    std::string("config key not found: ") + std::string(key),
                    "YamlConfig::GetString"));
  }

  try {
    return node.as<std::string>();
  } catch (const YAML::Exception& e) {
    return std::unexpected(
        Error::Make(ErrorCode::kInvalidArg,
                    std::string("type mismatch for key '") + std::string(key) +
                        "': " + e.what(),
                    "YamlConfig::GetString"));
  }
}

Result<int> YamlConfig::GetInt(std::string_view key) const {
  if (auto env = EnvOverride(key)) {
    try {
      return std::stoi(*env);
    } catch (...) {
      return std::unexpected(
          Error::Make(ErrorCode::kInvalidArg,
                      std::string("env var not an integer for key: ") +
                          std::string(key),
                      "YamlConfig::GetInt"));
    }
  }

  auto node = Resolve(key);
  if (!node || node.IsNull()) {
    return std::unexpected(
        Error::Make(ErrorCode::kNotFound,
                    std::string("config key not found: ") + std::string(key),
                    "YamlConfig::GetInt"));
  }

  try {
    return node.as<int>();
  } catch (const YAML::Exception& e) {
    return std::unexpected(
        Error::Make(ErrorCode::kInvalidArg,
                    std::string("type mismatch for key '") + std::string(key) +
                        "': " + e.what(),
                    "YamlConfig::GetInt"));
  }
}

Result<bool> YamlConfig::GetBool(std::string_view key) const {
  if (auto env = EnvOverride(key)) {
    std::string val = *env;
    std::transform(val.begin(), val.end(), val.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return val == "true" || val == "1" || val == "yes";
  }

  auto node = Resolve(key);
  if (!node || node.IsNull()) {
    return std::unexpected(
        Error::Make(ErrorCode::kNotFound,
                    std::string("config key not found: ") + std::string(key),
                    "YamlConfig::GetBool"));
  }

  try {
    return node.as<bool>();
  } catch (const YAML::Exception& e) {
    return std::unexpected(
        Error::Make(ErrorCode::kInvalidArg,
                    std::string("type mismatch for key '") + std::string(key) +
                        "': " + e.what(),
                    "YamlConfig::GetBool"));
  }
}

std::string YamlConfig::GetStringOr(std::string_view key,
                                    std::string_view default_val) const {
  auto result = GetString(key);
  return result ? *result : std::string(default_val);
}

int YamlConfig::GetIntOr(std::string_view key, int default_val) const {
  auto result = GetInt(key);
  return result ? *result : default_val;
}

bool YamlConfig::GetBoolOr(std::string_view key, bool default_val) const {
  auto result = GetBool(key);
  return result ? *result : default_val;
}

// ── 内部方法 ──────────────────────────────────────────────

YAML::Node YamlConfig::Resolve(std::string_view dotted_key) const {
  YAML::Node current = YAML::Clone(root_);
  std::istringstream stream(std::string(dotted_key));
  std::string segment;

  while (std::getline(stream, segment, '.')) {
    if (!current.IsMap() || !current[segment]) {
      return YAML::Node();
    }
    current = current[segment];
  }
  return current;
}

std::optional<std::string> YamlConfig::EnvOverride(std::string_view key) {
  std::string env_name = KeyToEnvName(key);
  const char* val = std::getenv(env_name.c_str());
  if (val != nullptr) {
    return std::string(val);
  }
  return std::nullopt;
}

std::string YamlConfig::KeyToEnvName(std::string_view key) {
  std::string result = "EVOCLAW_";
  for (char c : key) {
    if (c == '.') {
      result += '_';
    } else {
      result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
  }
  return result;
}

}  // namespace evoclaw::config
