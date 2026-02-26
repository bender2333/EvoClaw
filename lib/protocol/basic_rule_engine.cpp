#include "protocol/basic_rule_engine.h"

namespace evoclaw::protocol {

Result<ValidationResult> BasicRuleEngine::Validate(
    const nlohmann::json& payload,
    const nlohmann::json& rules) const {
  if (!rules.is_object()) {
    return std::unexpected(
        Error::Make(ErrorCode::kInvalidArg,
                    "rules must be a JSON object",
                    "BasicRuleEngine::Validate"));
  }

  ValidationResult result;

  for (auto& [field_name, field_rules] : rules.items()) {
    ValidateField(field_name, payload, field_rules, result);
  }

  return result;
}

void BasicRuleEngine::ValidateField(
    const std::string& field_name,
    const nlohmann::json& payload,
    const nlohmann::json& field_rules,
    ValidationResult& result) const {
  bool required = field_rules.value("required", false);
  bool field_exists = payload.contains(field_name);

  // required 检查
  if (required && !field_exists) {
    result.passed = false;
    result.violations.push_back(
        "field '" + field_name + "' is required but missing");
    return;
  }

  if (!field_exists) return;

  const auto& value = payload[field_name];

  // type 检查
  if (field_rules.contains("type")) {
    auto expected_type = field_rules["type"].get<std::string>();
    bool type_ok = false;
    if (expected_type == "string") type_ok = value.is_string();
    else if (expected_type == "number") type_ok = value.is_number();
    else if (expected_type == "boolean") type_ok = value.is_boolean();
    else if (expected_type == "array") type_ok = value.is_array();
    else if (expected_type == "object") type_ok = value.is_object();

    if (!type_ok) {
      result.passed = false;
      result.violations.push_back(
          "field '" + field_name + "' expected type '" + expected_type + "'");
      return;  // 类型不对，后续检查无意义
    }
  }

  // min / max（数值）
  if (value.is_number()) {
    double num = value.get<double>();
    if (field_rules.contains("min") && num < field_rules["min"].get<double>()) {
      result.passed = false;
      result.violations.push_back(
          "field '" + field_name + "' value " + std::to_string(num) +
          " < min " + std::to_string(field_rules["min"].get<double>()));
    }
    if (field_rules.contains("max") && num > field_rules["max"].get<double>()) {
      result.passed = false;
      result.violations.push_back(
          "field '" + field_name + "' value " + std::to_string(num) +
          " > max " + std::to_string(field_rules["max"].get<double>()));
    }
  }

  // min_length / max_length（字符串）
  if (value.is_string()) {
    auto len = value.get<std::string>().size();
    if (field_rules.contains("min_length") &&
        len < static_cast<size_t>(field_rules["min_length"].get<int>())) {
      result.passed = false;
      result.violations.push_back(
          "field '" + field_name + "' length " + std::to_string(len) +
          " < min_length " +
          std::to_string(field_rules["min_length"].get<int>()));
    }
    if (field_rules.contains("max_length") &&
        len > static_cast<size_t>(field_rules["max_length"].get<int>())) {
      result.passed = false;
      result.violations.push_back(
          "field '" + field_name + "' length " + std::to_string(len) +
          " > max_length " +
          std::to_string(field_rules["max_length"].get<int>()));
    }
  }

  // enum 白名单
  if (field_rules.contains("enum")) {
    const auto& allowed = field_rules["enum"];
    bool found = false;
    for (const auto& item : allowed) {
      if (item == value) {
        found = true;
        break;
      }
    }
    if (!found) {
      result.passed = false;
      result.violations.push_back(
          "field '" + field_name + "' value not in allowed enum");
    }
  }
}

}  // namespace evoclaw::protocol
