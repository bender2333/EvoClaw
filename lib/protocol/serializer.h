#ifndef EVOCLAW_PROTOCOL_SERIALIZER_H_
#define EVOCLAW_PROTOCOL_SERIALIZER_H_

#include <string>

#include <nlohmann/json.hpp>

#include "common/error.h"
#include "protocol/message.h"

namespace evoclaw::protocol {

/// JSON 序列化器 — Message ↔ JSON ↔ string
///
/// 所有 ZeroMQ 消息经过此类序列化/反序列化。
/// JSON 字段一律 snake_case。
class Serializer {
 public:
  /// Message → JSON string
  static Result<std::string> Serialize(const Message& msg);

  /// JSON string → Message
  static Result<Message> Deserialize(std::string_view json_str);

  /// Message → nlohmann::json
  static nlohmann::json ToJson(const Message& msg);

  /// nlohmann::json → Message
  static Result<Message> FromJson(const nlohmann::json& j);
};

}  // namespace evoclaw::protocol

#endif  // EVOCLAW_PROTOCOL_SERIALIZER_H_
