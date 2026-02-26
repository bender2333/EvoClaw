#ifndef EVOCLAW_CONFIG_CONFIG_H_
#define EVOCLAW_CONFIG_CONFIG_H_

#include <expected>
#include <string>
#include <vector>

namespace evoclaw {

struct Error {
  enum class Code { kOk, kNotFound, kTimeout, kInvalidArg, kInternal, kIoError };
  Code code;
  std::string message;
  std::string context;
};

/// 端点配置
struct EndpointConfig {
  std::string engine;     // e.g. "ipc:///tmp/evoclaw-engine.sock"
  std::string compiler;   // e.g. "ipc:///tmp/evoclaw-compiler.sock"
  std::string dashboard;  // e.g. "ipc:///tmp/evoclaw-dashboard.sock"
  std::string judge;      // e.g. "ipc:///tmp/evoclaw-judge.sock"
  std::string pub_sub;    // e.g. "ipc:///tmp/evoclaw-pubsub.sock"
};

/// LLM 配置
struct LlmConfig {
  std::string provider;   // "openai"
  std::string api_key;
  std::string base_url;   // "https://api.openai.com/v1"
  std::string model;      // "gpt-4"
  int max_retries = 3;
  int timeout_seconds = 30;
};

/// 预算配置
struct BudgetConfig {
  int64_t max_tokens_per_task = 100000;
  int64_t max_tokens_per_agent = 50000;
  int max_tool_calls_per_task = 50;
  int max_rounds_per_task = 20;
};

/// 全局配置
struct AppConfig {
  std::string process_name;
  std::string data_dir = "./data";
  std::string log_level = "INFO";
  EndpointConfig endpoints;
  LlmConfig llm;
  BudgetConfig budget;
};

/// 配置加载接口
class Config {
 public:
  virtual ~Config() = default;
  virtual std::expected<AppConfig, Error> Load(const std::string& path) = 0;
};

}  // namespace evoclaw

#endif  // EVOCLAW_CONFIG_CONFIG_H_
