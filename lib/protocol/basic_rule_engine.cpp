#include "protocol/basic_rule_engine.h"

namespace evoclaw {

std::expected<ValidationResult, Error> BasicRuleEngine::Validate(
    const nlohmann::json& payload,
    const nlohmann::json& constraints) {
  // MVP: 遍历约束列表，支持 eq/ne/gt/lt/gte/lte/range/in 操作
  if (!constraints.is_array()) {
    return ValidationResult{true, ""};
  }

  for (const auto& c : constraints) {
    if (!c.contains("field") || !c.contains("op")) continue;

    std::string field = c["field"].get<std::string>();
    std::string op = c["op"].get<std::string>();

    if (!payload.contains(field)) {
      if (c.value("required", false)) {
        return ValidationResult{false, "Missing required field: " + field};
      }
      continue;
    }

    const auto& val = payload[field];

    if (op == "eq" && c.contains("value")) {
      if (val != c["value"]) {
        return ValidationResult{false, field + " != expected value"};
      }
    } else if (op == "ne" && c.contains("value")) {
      if (val == c["value"]) {
        return ValidationResult{false, field + " == forbidden value"};
      }
    } else if (op == "gt" && c.contains("value") && val.is_number() && c["value"].is_number()) {
      if (val.get<double>() <= c["value"].get<double>()) {
        return ValidationResult{false, field + " not > threshold"};
      }
    } else if (op == "lt" && c.contains("value") && val.is_number() && c["value"].is_number()) {
      if (val.get<double>() >= c["value"].get<double>()) {
        return ValidationResult{false, field + " not < threshold"};
      }
    } else if (op == "gte" && c.contains("value") && val.is_number() && c["value"].is_number()) {
      if (val.get<double>() < c["value"].get<double>()) {
        return ValidationResult{false, field + " not >= threshold"};
      }
    } else if (op == "lte" && c.contains("value") && val.is_number() && c["value"].is_number()) {
      if (val.get<double>() > c["value"].get<double>()) {
        return ValidationResult{false, field + " not <= threshold"};
      }
    } else if (op == "range" && c.contains("min") && c.contains("max") && val.is_number()) {
      double v = val.get<double>();
      if (v < c["min"].get<double>() || v > c["max"].get<double>()) {
        return ValidationResult{false, field + " out of range"};
      }
    } else if (op == "in" && c.contains("values") && c["values"].is_array()) {
      bool found = false;
      for (const auto& allowed : c["values"]) {
        if (val == allowed) { found = true; break; }
      }
      if (!found) {
        return ValidationResult{false, field + " not in allowed values"};
      }
    }
  }

  return ValidationResult{true, ""};
}

}  // namespace evoclaw
