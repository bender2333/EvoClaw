#include "llm/prompt_registry.h"
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace evoclaw {

std::expected<PromptMeta, Error> PromptRegistry::LoadPrompt(
    const std::filesystem::path& yaml_path) const {
  try {
    YAML::Node root = YAML::LoadFile(yaml_path.string());
    PromptMeta meta;
    meta.id = root["id"].as<std::string>();
    meta.version = root["version"] ? root["version"].as<int>() : 1;
    meta.output_schema = root["output_schema"] ? root["output_schema"].as<std::string>() : "text";

    if (root["depends_on"]) {
      for (const auto& dep : root["depends_on"]) {
        meta.depends_on.push_back(dep.as<std::string>());
      }
    }

    // 加载对应的 .txt 模板文件
    auto txt_path = yaml_path;
    txt_path.replace_extension(".txt");
    if (std::filesystem::exists(txt_path)) {
      std::ifstream ifs(txt_path);
      meta.template_text = std::string(
          std::istreambuf_iterator<char>(ifs),
          std::istreambuf_iterator<char>());
    }

    return meta;
  } catch (const std::exception& e) {
    return std::unexpected(Error{Error::Code::kInvalidArg, e.what(), "PromptRegistry::LoadPrompt"});
  }
}

std::expected<void, Error> PromptRegistry::LoadFromDir(const std::filesystem::path& prompts_dir) {
  if (!std::filesystem::exists(prompts_dir)) return {};

  for (const auto& entry : std::filesystem::recursive_directory_iterator(prompts_dir)) {
    if (entry.path().extension() == ".yaml") {
      auto meta = LoadPrompt(entry.path());
      if (meta) {
        prompts_[meta->id] = std::move(*meta);
      }
    }
  }

  return ValidateDependencies();
}

std::expected<void, Error> PromptRegistry::ValidateDependencies() const {
  for (const auto& [id, meta] : prompts_) {
    for (const auto& dep : meta.depends_on) {
      if (!prompts_.contains(dep)) {
        return std::unexpected(Error{Error::Code::kInvalidArg,
            "Prompt '" + id + "' depends on missing prompt '" + dep + "'",
            "PromptRegistry::ValidateDependencies"});
      }
    }
  }
  return {};
}

std::expected<PromptMeta, Error> PromptRegistry::Get(const std::string& id) const {
  if (auto it = prompts_.find(id); it != prompts_.end()) {
    return it->second;
  }
  return std::unexpected(Error{Error::Code::kNotFound, "Prompt not found: " + id, "PromptRegistry::Get"});
}

std::expected<std::string, Error> PromptRegistry::Render(
    const std::string& id,
    const std::unordered_map<std::string, std::string>& vars) const {
  auto meta = Get(id);
  if (!meta) return std::unexpected(meta.error());

  std::string result = meta->template_text;
  for (const auto& [key, value] : vars) {
    std::string placeholder = "{{" + key + "}}";
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
      result.replace(pos, placeholder.length(), value);
      pos += value.length();
    }
  }
  return result;
}

std::vector<std::string> PromptRegistry::ListIds() const {
  std::vector<std::string> ids;
  ids.reserve(prompts_.size());
  for (const auto& [id, _] : prompts_) ids.push_back(id);
  return ids;
}

}  // namespace evoclaw
