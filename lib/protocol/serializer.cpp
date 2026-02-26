#include "protocol/serializer.h"

#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace evoclaw::protocol {

// ── Message 静态方法 ──────────────────────────────────────

std::string Message::Now() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &time_t);
#else
  gmtime_r(&time_t, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

std::string Message::NewId() {
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

  auto hex = [&](int bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < bytes; ++i) {
      oss << std::setw(2) << (dist(gen) & 0xFF);
    }
    return oss.str();
  };

  // UUID v4 格式：8-4-4-4-12
  return hex(4) + "-" + hex(2) + "-4" + hex(1).substr(0, 3) + "-" +
         hex(2) + "-" + hex(6);
}

// ── Serializer ────────────────────────────────────────────

nlohmann::json Serializer::ToJson(const Message& msg) {
  return {
      {"msg_id", msg.msg_id},
      {"msg_type", msg.msg_type},
      {"source", msg.source},
      {"target", msg.target},
      {"timestamp", msg.timestamp},
      {"qos", static_cast<int>(msg.qos)},
      {"payload", msg.payload},
  };
}

Result<std::string> Serializer::Serialize(const Message& msg) {
  try {
    return ToJson(msg).dump();
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(
        Error::Make(ErrorCode::kSerializationError,
                    std::string("JSON serialize failed: ") + e.what(),
                    "Serializer::Serialize"));
  }
}

Result<Message> Serializer::FromJson(const nlohmann::json& j) {
  try {
    Message msg;
    msg.msg_id = j.at("msg_id").get<std::string>();
    msg.msg_type = j.at("msg_type").get<std::string>();
    msg.source = j.at("source").get<std::string>();
    msg.target = j.value("target", "");
    msg.timestamp = j.at("timestamp").get<std::string>();
    msg.qos = static_cast<QoS>(j.value("qos", 0));
    msg.payload = j.value("payload", nlohmann::json::object());
    return msg;
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(
        Error::Make(ErrorCode::kSerializationError,
                    std::string("JSON deserialize failed: ") + e.what(),
                    "Serializer::FromJson"));
  }
}

Result<Message> Serializer::Deserialize(std::string_view json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    return FromJson(j);
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(
        Error::Make(ErrorCode::kSerializationError,
                    std::string("JSON parse failed: ") + e.what(),
                    "Serializer::Deserialize"));
  }
}

}  // namespace evoclaw::protocol
