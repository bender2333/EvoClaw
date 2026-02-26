#ifndef EVOCLAW_PROTOCOL_MESSAGE_H_
#define EVOCLAW_PROTOCOL_MESSAGE_H_

#include <chrono>
#include <string>
#include <nlohmann/json.hpp>

namespace evoclaw {

/// ZeroMQ 消息信封
struct Message {
  std::string msg_id;
  std::string msg_type;     // 点分层级: "task.request", "task.response", "heartbeat"
  std::string source;       // 发送进程: "engine", "compiler", "dashboard", "judge"
  std::string target;       // 目标进程（Pub-Sub 时为空）
  std::string timestamp;    // ISO 8601
  nlohmann::json payload;   // 业务数据
};

/// 事件日志条目
struct Event {
  std::string event_id;
  std::string event_type;       // 点分层级: "agent.task.completed"
  std::string source_process;
  std::string timestamp;
  nlohmann::json data;
  std::string checksum;         // "sha256:..."
};

}  // namespace evoclaw

#endif  // EVOCLAW_PROTOCOL_MESSAGE_H_
