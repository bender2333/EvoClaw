#ifndef EVOCLAW_COMMON_ERROR_H_
#define EVOCLAW_COMMON_ERROR_H_

#include <expected>
#include <string>
#include <string_view>

namespace evoclaw {

/// 统一错误码
enum class ErrorCode {
  kOk,
  kNotFound,
  kTimeout,
  kInvalidArg,
  kIOError,
  kSerializationError,
  kConnectionFailed,
  kPermissionDenied,
  kInternal,
};

/// 统一错误类型 — 所有可失败函数返回 std::expected<T, Error>
struct Error {
  ErrorCode code;
  std::string message;
  std::string context;  // 模块名/函数名

  /// 从模块级错误码构造
  template <typename ModuleError>
  static Error From(ModuleError e, std::string_view ctx) {
    return {ErrorCode::kInternal, ModuleErrorToString(e), std::string(ctx)};
  }

  /// 快捷构造
  static Error Make(ErrorCode code, std::string_view msg,
                    std::string_view ctx = "") {
    return {code, std::string(msg), std::string(ctx)};
  }
};

/// 通用 Result 类型别名
template <typename T>
using Result = std::expected<T, Error>;

/// void 返回值的 Result
using Status = std::expected<void, Error>;

}  // namespace evoclaw

#endif  // EVOCLAW_COMMON_ERROR_H_
