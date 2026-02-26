#ifndef EVOCLAW_PROTOCOL_SERIALIZER_H_
#define EVOCLAW_PROTOCOL_SERIALIZER_H_

#include "protocol/message.h"
#include "config/config.h"
#include <expected>
#include <string>

namespace evoclaw {

/// JSON 序列化/反序列化
class Serializer {
 public:
  /// Message → JSON 字符串
  static std::string Serialize(const Message& msg);

  /// JSON 字符串 → Message
  static std::expected<Message, Error> Deserialize(const std::string& json_str);

  /// Event → JSON 字符串（含 checksum 计算）
  static std::string SerializeEvent(const Event& event);

  /// JSON 字符串 → Event（含 checksum 校验）
  static std::expected<Event, Error> DeserializeEvent(const std::string& json_str);
};

}  // namespace evoclaw

#endif  // EVOCLAW_PROTOCOL_SERIALIZER_H_
