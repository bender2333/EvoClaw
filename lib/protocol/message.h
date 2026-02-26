#ifndef EVOCLAW_PROTOCOL_MESSAGE_H_
#define EVOCLAW_PROTOCOL_MESSAGE_H_

#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

namespace evoclaw::protocol {

/// QoS 级别
enum class QoS {
  kFireAndForget = 0,  // 无确认（心跳/日志）
  kAtLeastOnce = 1,    // msg_id 幂等去重
};

/// 消息信封 — ZeroMQ 消息的标准格式
///
/// 架构文档定义：
///   msg_id / msg_type / source / target / timestamp / payload
///   QoS 两级：fire-and-forget / at-least-once
struct Message {
  std::string msg_id;       // UUID
  std::string msg_type;     // 点分层级，如 "task.request"
  std::string source;       // 发送进程，如 "engine"
  std::string target;       // 目标进程，如 "compiler"（空 = 广播）
  std::string timestamp;    // ISO 8601，如 "2026-02-26T10:30:00Z"
  QoS qos = QoS::kFireAndForget;
  nlohmann::json payload;   // 业务数据

  /// 生成当前 ISO 8601 时间戳
  static std::string Now();

  /// 生成 UUID v4
  static std::string NewId();
};

}  // namespace evoclaw::protocol

#endif  // EVOCLAW_PROTOCOL_MESSAGE_H_
