#include <gtest/gtest.h>
#include "protocol/serializer.h"
#include "common/uuid.h"

using namespace evoclaw;

TEST(SerializerTest, SerializeDeserializeMessage) {
  Message msg;
  msg.msg_id = GenerateUuid();
  msg.msg_type = "task.request";
  msg.source = "engine";
  msg.target = "compiler";
  msg.timestamp = "2026-02-26T10:30:00Z";
  msg.payload = {{"task", "compile"}, {"priority", 1}};

  std::string json = Serializer::Serialize(msg);
  auto result = Serializer::Deserialize(json);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->msg_id, msg.msg_id);
  EXPECT_EQ(result->msg_type, "task.request");
  EXPECT_EQ(result->source, "engine");
  EXPECT_EQ(result->target, "compiler");
  EXPECT_EQ(result->payload["task"], "compile");
}

TEST(SerializerTest, DeserializeInvalidJson) {
  auto result = Serializer::Deserialize("not json");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, Error::Code::kInvalidArg);
}

TEST(SerializerTest, SerializeDeserializeEvent) {
  Event event;
  event.event_id = GenerateUuid();
  event.event_type = "agent.task.completed";
  event.source_process = "engine";
  event.timestamp = "2026-02-26T10:30:00Z";
  event.data = {{"result", "success"}};

  std::string json = Serializer::SerializeEvent(event);
  auto result = Serializer::DeserializeEvent(json);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->event_id, event.event_id);
  EXPECT_EQ(result->event_type, "agent.task.completed");
  EXPECT_FALSE(result->checksum.empty());
}

TEST(SerializerTest, EventChecksumTamperDetection) {
  Event event;
  event.event_id = "test-id";
  event.event_type = "test.event";
  event.source_process = "engine";
  event.timestamp = "2026-02-26T10:30:00Z";
  event.data = {{"key", "value"}};

  std::string json = Serializer::SerializeEvent(event);
  // 篡改 payload
  auto j = nlohmann::json::parse(json);
  j["data"]["key"] = "tampered";
  std::string tampered = j.dump();

  auto result = Serializer::DeserializeEvent(tampered);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, Error::Code::kInvalidArg);
}
