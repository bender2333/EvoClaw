#include "governance/constitution.h"
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace evoclaw {

std::expected<void, Error> Constitution::LoadFromFile(const std::string& path) {
  try {
    YAML::Node config = YAML::LoadFile(path);
    std::lock_guard lock(mutex_);
    rules_.clear();
    if (config["rules"] && config["rules"].IsSequence()) {
      for (const auto& node : config["rules"]) {
        ConstitutionRule rule;
        rule.id = node["id"].as<std::string>("");
        rule.description = node["description"].as<std::string>("");
        rule.forbidden_action = node["forbidden_action"].as<std::string>("");
        rules_.push_back(std::move(rule));
      }
    }
    return {};
  } catch (const std::exception& e) {
    return std::unexpected(Error{Error::Code::kIoError, e.what(), "Constitution::LoadFromFile"});
  }
}

void Constitution::AddRule(const ConstitutionRule& rule) {
  std::lock_guard lock(mutex_);
  rules_.push_back(rule);
}

std::expected<bool, Error> Constitution::Check(const Message& action) const {
  std::lock_guard lock(mutex_);
  for (const auto& rule : rules_) {
    // 前缀匹配：如果操作的 msg_type 以禁止的 action 开头，则违反宪法
    if (!rule.forbidden_action.empty() &&
        action.msg_type.starts_with(rule.forbidden_action)) {
      return false;  // 违反宪法
    }
  }
  return true;  // 合规
}

std::expected<std::string, Error> Constitution::ProposeAmendment(const ConstitutionRule& rule) {
  std::lock_guard lock(mutex_);
  std::string id = "amendment_" + std::to_string(next_amendment_id_++);
  Amendment amendment;
  amendment.id = id;
  amendment.rule = rule;
  amendment.proposed_at = std::chrono::system_clock::now();
  amendments_[id] = std::move(amendment);
  return id;
}

std::expected<bool, Error> Constitution::ConfirmAmendment(const std::string& amendment_id) {
  std::lock_guard lock(mutex_);
  auto it = amendments_.find(amendment_id);
  if (it == amendments_.end()) {
    return std::unexpected(Error{Error::Code::kNotFound,
        "Amendment not found: " + amendment_id, "Constitution::ConfirmAmendment"});
  }

  auto& amendment = it->second;
  auto elapsed = std::chrono::system_clock::now() - amendment.proposed_at;
  if (elapsed < Amendment::kCooldownPeriod) {
    return false;  // 冷静期未过
  }

  // 冷静期已过，应用修正案
  amendment.confirmed = true;
  rules_.push_back(amendment.rule);
  return true;
}

size_t Constitution::GetRuleCount() const {
  std::lock_guard lock(mutex_);
  return rules_.size();
}

}  // namespace evoclaw
