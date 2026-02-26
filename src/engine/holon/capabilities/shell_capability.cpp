#include "holon/capabilities/shell_capability.h"
#include <array>
#include <cstdio>
#include <memory>

namespace evoclaw {

ShellCapability::ShellCapability() {
  // 危险命令黑名单
  blacklist_ = {
      "rm -rf /", "rm -rf /*", "mkfs", "dd if=",
      ":(){:|:&};:", "chmod -R 777 /", "chown -R",
      "> /dev/sda", "mv / ", "wget | sh", "curl | sh",
  };
}

bool ShellCapability::IsDangerous(const std::string& command) const {
  for (const auto& pattern : blacklist_) {
    if (command.find(pattern) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::expected<nlohmann::json, Error> ShellCapability::Execute(
    const nlohmann::json& params) {
  std::string command = params.value("command", "");
  if (command.empty()) {
    return std::unexpected(Error{Error::Code::kInvalidArg,
        "Missing 'command' parameter", "ShellCapability::Execute"});
  }

  // 安全检查
  if (IsDangerous(command)) {
    return std::unexpected(Error{Error::Code::kInvalidArg,
        "Dangerous command blocked: " + command, "ShellCapability::Execute"});
  }

  // 执行命令并捕获输出
  std::array<char, 4096> buffer;
  std::string output;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(
      popen(command.c_str(), "r"), pclose);

  if (!pipe) {
    return std::unexpected(Error{Error::Code::kInternal,
        "Failed to execute command", "ShellCapability::Execute"});
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    output += buffer.data();
  }

  int exit_code = pclose(pipe.release());

  return nlohmann::json{
      {"output", output},
      {"exit_code", WEXITSTATUS(exit_code)},
  };
}

}  // namespace evoclaw
