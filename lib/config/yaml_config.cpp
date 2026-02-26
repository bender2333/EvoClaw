#include "config/yaml_config.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace evoclaw {

std::expected<AppConfig, Error> YamlConfig::Load(const std::string& path) {
  try {
    YAML::Node root = YAML::LoadFile(path);
    AppConfig cfg;

    if (root["process_name"]) cfg.process_name = root["process_name"].as<std::string>();
    if (root["data_dir"]) cfg.data_dir = root["data_dir"].as<std::string>();
    if (root["log_level"]) cfg.log_level = root["log_level"].as<std::string>();

    if (auto ep = root["endpoints"]) {
      if (ep["engine"]) cfg.endpoints.engine = ep["engine"].as<std::string>();
      if (ep["compiler"]) cfg.endpoints.compiler = ep["compiler"].as<std::string>();
      if (ep["dashboard"]) cfg.endpoints.dashboard = ep["dashboard"].as<std::string>();
      if (ep["judge"]) cfg.endpoints.judge = ep["judge"].as<std::string>();
      if (ep["pub_sub"]) cfg.endpoints.pub_sub = ep["pub_sub"].as<std::string>();
    }

    if (auto llm = root["llm"]) {
      if (llm["provider"]) cfg.llm.provider = llm["provider"].as<std::string>();
      if (llm["api_key"]) cfg.llm.api_key = llm["api_key"].as<std::string>();
      if (llm["base_url"]) cfg.llm.base_url = llm["base_url"].as<std::string>();
      if (llm["model"]) cfg.llm.model = llm["model"].as<std::string>();
      if (llm["max_retries"]) cfg.llm.max_retries = llm["max_retries"].as<int>();
      if (llm["timeout_seconds"]) cfg.llm.timeout_seconds = llm["timeout_seconds"].as<int>();
    }

    if (auto budget = root["budget"]) {
      if (budget["max_tokens_per_task"]) cfg.budget.max_tokens_per_task = budget["max_tokens_per_task"].as<int64_t>();
      if (budget["max_tokens_per_agent"]) cfg.budget.max_tokens_per_agent = budget["max_tokens_per_agent"].as<int64_t>();
      if (budget["max_tool_calls_per_task"]) cfg.budget.max_tool_calls_per_task = budget["max_tool_calls_per_task"].as<int>();
      if (budget["max_rounds_per_task"]) cfg.budget.max_rounds_per_task = budget["max_rounds_per_task"].as<int>();
    }

    return cfg;
  } catch (const YAML::Exception& e) {
    return std::unexpected(Error{Error::Code::kInvalidArg, e.what(), "YamlConfig::Load"});
  } catch (const std::exception& e) {
    return std::unexpected(Error{Error::Code::kIoError, e.what(), "YamlConfig::Load"});
  }
}

}  // namespace evoclaw
