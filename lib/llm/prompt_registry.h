#ifndef EVOCLAW_LLM_PROMPT_REGISTRY_H_
#define EVOCLAW_LLM_PROMPT_REGISTRY_H_

#include "config/config.h"
#include <expected>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace evoclaw {

/// Prompt 模板元数据
struct PromptMeta {
  std::string id;                          // e.g. "compiler.pattern_recognition"
  int version = 1;
  std::vector<std::string> depends_on;     // 依赖的其他 prompt ID
  std::string output_schema;               // "json" | "text"
  std::string template_text;               // 模板正文
};

/// Prompt Registry — 管理所有 prompt 模板
/// 加载时验证依赖关系，支持版本回滚
class PromptRegistry {
 public:
  /// 从目录加载所有 prompt（YAML 元数据 + txt 模板）
  std::expected<void, Error> LoadFromDir(const std::filesystem::path& prompts_dir);

  /// 获取 prompt 模板
  std::expected<PromptMeta, Error> Get(const std::string& id) const;

  /// 渲染模板（替换占位符）
  std::expected<std::string, Error> Render(
      const std::string& id,
      const std::unordered_map<std::string, std::string>& vars) const;

  /// 获取所有已注册的 prompt ID
  std::vector<std::string> ListIds() const;

 private:
  std::unordered_map<std::string, PromptMeta> prompts_;

  std::expected<PromptMeta, Error> LoadPrompt(
      const std::filesystem::path& yaml_path) const;
  std::expected<void, Error> ValidateDependencies() const;
};

}  // namespace evoclaw

#endif  // EVOCLAW_LLM_PROMPT_REGISTRY_H_
