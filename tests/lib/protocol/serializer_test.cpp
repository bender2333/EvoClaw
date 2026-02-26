#include <gtest/gtest.h>

#include "protocol/message.h"
#include "protocol/message_types.h"
#include "protocol/serializer.h"

namespace evoclaw::protocol {
namespace {

// ── 序列化 ────────────────────────────────────────────────

TEST(SerializerTest, Serialize_ValidMessage_ReturnsJsonString) {
  Message msg;
  msg.msg_id = "test-id-001";
  msg.msg_type = std::string(msg_type::kTaskRequest);
  msg.source = std::string(process::kEngine);
  msg.target = std::string(process::kCompiler);
  msg.timestamp = "2026-02-26T10:30:00Z";
  msg.qos = QoS::kAtLeastOnce;
  msg.payload = {{"task", "summarize"}, {"priority", 1}};

  auto result = Serializer::Serialize(msg);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  // 反序列化验证 JSON 结构
  auto j = nlohmann::json::parse(*result);
  EXPECT_EQ(j["msg_id"], "test-id-001");
  EXPECT_EQ(j["msg_type"], "task.request");
  EXPECT_EQ(j["source"], "engine");
  EXPECT_EQ(j["target"], "compiler");
  EXPECT_EQ(j["qos"], 1);
  EXPECT_EQ(j["payload"]["task"], "summarize");
}

// ── 反序列化 ──────────────────────────────────────────────

TEST(SerializerTest, Deserialize_ValidJson_ReturnsMessage) {
  std::string json_str = R"({
    "msg_id": "abc-123",
    "msg_type": "system.heartbeat",
    "source": "engine",
    "target": "",
    "timestamp": "2026-02-26T10:30:00Z",
    "qos": 0,
    "payload": {}
  })";

  auto result = Serializer::Deserialize(json_str);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  EXPECT_EQ(result->msg_id, "abc-123");
  EXPECT_EQ(result->msg_type, "system.heartbeat");
  EXPECT_EQ(result->source, "engine");
  EXPECT_EQ(result->target, "");
  EXPECT_EQ(result->qos, QoS::kFireAndForget);
}

TEST(SerializerTest, Deserialize_MissingOptionalFields_UsesDefaults) {
  // target 和 payload 可选
  std::string json_str = R"({
    "msg_id": "abc-123",
    "msg_type": "system.heartbeat",
    "source": "engine",
    "timestamp": "2026-02-26T10:30:00Z"
  })";

  auto result = Serializer::Deserialize(json_str);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  EXPECT_EQ(result->target, "");
  EXPECT_EQ(result->qos, QoS::kFireAndForget);
  EXPECT_TRUE(result->payload.is_object());
}

// ── 往返一致性 ────────────────────────────────────────────

TEST(SerializerTest, RoundTrip_SerializeDeserialize_Consistent) {
  Message original;
  original.msg_id = Message::NewId();
  original.msg_type = std::string(msg_type::kCompileRequest);
  original.source = std::string(process::kEngine);
  original.target = std::string(process::kCompiler);
  original.timestamp = Message::Now();
  original.qos = QoS::kAtLeastOnce;
  original.payload = {{"pattern_id", "p-001"}, {"count", 5}};

  auto serialized = Serializer::Serialize(original);
  ASSERT_TRUE(serialized.has_value());

  auto deserialized = Serializer::Deserialize(*serialized);
  ASSERT_TRUE(deserialized.has_value());

  EXPECT_EQ(deserialized->msg_id, original.msg_id);
  EXPECT_EQ(deserialized->msg_type, original.msg_type);
  EXPECT_EQ(deserialized->source, original.source);
  EXPECT_EQ(deserialized->target, original.target);
  EXPECT_EQ(deserialized->timestamp, original.timestamp);
  EXPECT_EQ(deserialized->qos, original.qos);
  EXPECT_EQ(deserialized->payload, original.payload);
}

// ── 错误处理 ──────────────────────────────────────────────

TEST(SerializerTest, Deserialize_InvalidJson_ReturnsError) {
  auto result = Serializer::Deserialize("{{{invalid json");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ErrorCode::kSerializationError);
}

TEST(SerializerTest, Deserialize_MissingRequiredField_ReturnsError) {
  // 缺少 msg_id
  std::string json_str = R"({
    "msg_type": "system.heartbeat",
    "source": "engine",
    "timestamp": "2026-02-26T10:30:00Z"
  })";

  auto result = Serializer::Deserialize(json_str);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ErrorCode::kSerializationError);
}

// ── Message 辅助方法 ─────────────────────────────────────

TEST(MessageTest, NewId_GeneratesNonEmptyString) {
  auto id = Message::NewId();
  EXPECT_FALSE(id.empty());
  // UUID 格式包含 '-'
  EXPECT_NE(id.find('-'), std::string::npos);
}

TEST(MessageTest, NewId_GeneratesUniqueIds) {
  auto id1 = Message::NewId();
  auto id2 = Message::NewId();
  EXPECT_NE(id1, id2);
}

TEST(MessageTest, Now_ReturnsIso8601Format) {
  auto ts = Message::Now();
  EXPECT_FALSE(ts.empty());
  // 基本格式检查：包含 T 和 Z
  EXPECT_NE(ts.find('T'), std::string::npos);
  EXPECT_EQ(ts.back(), 'Z');
}

// ── 消息类型常量 ─────────────────────────────────────────

TEST(MessageTypesTest, Constants_AreCorrectValues) {
  EXPECT_EQ(msg_type::kHeartbeat, "system.heartbeat");
  EXPECT_EQ(msg_type::kTaskRequest, "task.request");
  EXPECT_EQ(msg_type::kCompileRequest, "evolution.compile.request");
  EXPECT_EQ(process::kEngine, "engine");
  EXPECT_EQ(process::kCompiler, "compiler");
}

}  // namespace
}  // namespace evoclaw::protocol
