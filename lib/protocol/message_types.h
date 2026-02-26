#ifndef EVOCLAW_PROTOCOL_MESSAGE_TYPES_H_
#define EVOCLAW_PROTOCOL_MESSAGE_TYPES_H_

#include <string_view>

namespace evoclaw::protocol {

/// 消息类型常量 — 点分层级命名
///
/// 命名规范：{domain}.{action}
///   domain: system / task / evolution / agent
///   action: 具体操作
namespace msg_type {

// ── 系统级 ─────────────────────────────────────────────
constexpr std::string_view kHeartbeat = "system.heartbeat";
constexpr std::string_view kShutdown = "system.shutdown";
constexpr std::string_view kHealthCheck = "system.health_check";
constexpr std::string_view kHealthResponse = "system.health_response";

// ── 任务级 ─────────────────────────────────────────────
constexpr std::string_view kTaskRequest = "task.request";
constexpr std::string_view kTaskResponse = "task.response";
constexpr std::string_view kTaskProgress = "task.progress";
constexpr std::string_view kTaskCancel = "task.cancel";

// ── 进化级 ─────────────────────────────────────────────
constexpr std::string_view kCompileRequest = "evolution.compile.request";
constexpr std::string_view kCompileResult = "evolution.compile.result";
constexpr std::string_view kPatternDetected = "evolution.pattern.detected";

// ── Agent 级 ───────────────────────────────────────────
constexpr std::string_view kAgentSpawned = "agent.spawned";
constexpr std::string_view kAgentCompleted = "agent.completed";
constexpr std::string_view kAgentFailed = "agent.failed";

}  // namespace msg_type

/// 进程名常量
namespace process {

constexpr std::string_view kEngine = "engine";
constexpr std::string_view kCompiler = "compiler";
constexpr std::string_view kDashboard = "dashboard";
constexpr std::string_view kJudge = "judge";

}  // namespace process

}  // namespace evoclaw::protocol

#endif  // EVOCLAW_PROTOCOL_MESSAGE_TYPES_H_
