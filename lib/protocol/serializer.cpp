#include "protocol/serializer.h"
#include "common/uuid.h"
#include "common/sha256.h"
#include <sstream>
#include <iomanip>

namespace evoclaw {

namespace {

std::string ComputeSha256(const std::string& data) {
  return "sha256:" + Sha256::Hash(data);
}

}  // namespace

std::string Serializer::Serialize(const Message& msg) {
  nlohmann::json j;
  j["msg_id"] = msg.msg_id;
  j["msg_type"] = msg.msg_type;
  j["source"] = msg.source;
  j["target"] = msg.target;
  j["timestamp"] = msg.timestamp;
  j["payload"] = msg.payload;
  return j.dump();
}

std::expected<Message, Error> Serializer::Deserialize(const std::string& json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    Message msg;
    msg.msg_id = j.at("msg_id").get<std::string>();
    msg.msg_type = j.at("msg_type").get<std::string>();
    msg.source = j.at("source").get<std::string>();
    msg.target = j.value("target", "");
    msg.timestamp = j.at("timestamp").get<std::string>();
    msg.payload = j.value("payload", nlohmann::json::object());
    return msg;
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(Error{Error::Code::kInvalidArg, e.what(), "Serializer::Deserialize"});
  }
}

std::string Serializer::SerializeEvent(const Event& event) {
  nlohmann::json j;
  j["event_id"] = event.event_id;
  j["event_type"] = event.event_type;
  j["source_process"] = event.source_process;
  j["timestamp"] = event.timestamp;
  j["data"] = event.data;

  // checksum 基于除 checksum 外的所有字段
  std::string content = j.dump();
  j["checksum"] = ComputeSha256(content);
  return j.dump();
}

std::expected<Event, Error> Serializer::DeserializeEvent(const std::string& json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    Event event;
    event.event_id = j.at("event_id").get<std::string>();
    event.event_type = j.at("event_type").get<std::string>();
    event.source_process = j.at("source_process").get<std::string>();
    event.timestamp = j.at("timestamp").get<std::string>();
    event.data = j.value("data", nlohmann::json::object());
    event.checksum = j.value("checksum", "");

    // 校验 checksum
    if (!event.checksum.empty()) {
      nlohmann::json verify_j;
      verify_j["event_id"] = event.event_id;
      verify_j["event_type"] = event.event_type;
      verify_j["source_process"] = event.source_process;
      verify_j["timestamp"] = event.timestamp;
      verify_j["data"] = event.data;
      std::string expected_checksum = ComputeSha256(verify_j.dump());
      if (event.checksum != expected_checksum) {
        return std::unexpected(Error{Error::Code::kInvalidArg, "Checksum mismatch", "Serializer::DeserializeEvent"});
      }
    }

    return event;
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(Error{Error::Code::kInvalidArg, e.what(), "Serializer::DeserializeEvent"});
  }
}

}  // namespace evoclaw
