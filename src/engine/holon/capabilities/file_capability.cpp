#include "holon/capabilities/file_capability.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace evoclaw {

std::expected<nlohmann::json, Error> FileCapability::Execute(
    const nlohmann::json& params) {
  std::string action = params.value("action", "");
  std::string path = params.value("path", "");

  if (path.empty()) {
    return std::unexpected(Error{Error::Code::kInvalidArg,
        "Missing 'path' parameter", "FileCapability::Execute"});
  }

  if (action == "read") {
    std::ifstream ifs(path);
    if (!ifs) {
      return std::unexpected(Error{Error::Code::kNotFound,
          "File not found: " + path, "FileCapability::Execute"});
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    return nlohmann::json{{"content", ss.str()}, {"size", ss.str().size()}};
  }

  if (action == "write") {
    std::string content = params.value("content", "");
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream ofs(path);
    if (!ofs) {
      return std::unexpected(Error{Error::Code::kIoError,
          "Cannot write: " + path, "FileCapability::Execute"});
    }
    ofs << content;
    return nlohmann::json{{"written", content.size()}};
  }

  if (action == "list") {
    if (!std::filesystem::exists(path)) {
      return std::unexpected(Error{Error::Code::kNotFound,
          "Directory not found: " + path, "FileCapability::Execute"});
    }
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
      entries.push_back({
          {"name", entry.path().filename().string()},
          {"is_dir", entry.is_directory()},
      });
    }
    return nlohmann::json{{"entries", entries}};
  }

  if (action == "stat") {
    if (!std::filesystem::exists(path)) {
      return std::unexpected(Error{Error::Code::kNotFound,
          "Not found: " + path, "FileCapability::Execute"});
    }
    auto status = std::filesystem::status(path);
    return nlohmann::json{
        {"exists", true},
        {"is_file", std::filesystem::is_regular_file(status)},
        {"is_dir", std::filesystem::is_directory(status)},
        {"size", std::filesystem::is_regular_file(status)
                     ? static_cast<int64_t>(std::filesystem::file_size(path))
                     : 0},
    };
  }

  return std::unexpected(Error{Error::Code::kInvalidArg,
      "Unknown action: " + action, "FileCapability::Execute"});
}

}  // namespace evoclaw
