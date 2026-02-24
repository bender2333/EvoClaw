#include "protocol/bus.hpp"
#include "protocol/message.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>

namespace {

using evoclaw::protocol::Message;
using evoclaw::protocol::MessageBus;
using evoclaw::protocol::Performative;

TEST(ProtocolMessageTest, SerializeDeserializeRoundTrip) {
    Message msg;
    msg.performative = Performative::REQUEST;
    msg.sender = "planner";
    msg.receivers = {"executor", "critic"};
    msg.payload = { {"task", "implement phase1"}, {"priority", 1} };
    msg.metadata = { {"trace_id", "trace-001"} };

    const std::string json = msg.to_json();
    const auto parsed = Message::from_json(json);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->id, msg.id);
    EXPECT_EQ(parsed->performative, Performative::REQUEST);
    EXPECT_EQ(parsed->sender, "planner");
    EXPECT_EQ(parsed->receivers.size(), 2U);
    EXPECT_EQ(parsed->payload.at("task"), "implement phase1");
    EXPECT_EQ(parsed->metadata.at("trace_id"), "trace-001");
}

TEST(ProtocolMessageTest, InvalidJsonReturnsNullopt) {
    const auto parsed = Message::from_json("{not valid json}");
    EXPECT_FALSE(parsed.has_value());
}

TEST(ProtocolMessageTest, PerformativeEnumJsonMapping) {
    const nlohmann::json serialized = Performative::CRITIQUE;
    ASSERT_TRUE(serialized.is_string());
    EXPECT_EQ(serialized.get<std::string>(), "critique");

    const auto parsed = serialized.get<Performative>();
    EXPECT_EQ(parsed, Performative::CRITIQUE);
}

TEST(MessageBusTest, SendAndDeduplicate) {
    MessageBus bus;
    std::atomic<int> received = 0;

    bus.subscribe("executor", [&](const Message& incoming) {
        EXPECT_EQ(incoming.sender, "planner");
        ++received;
    });

    Message msg;
    msg.performative = Performative::REQUEST;
    msg.sender = "planner";
    msg.receivers = {"executor"};
    msg.payload = { {"key", "value"} };

    EXPECT_TRUE(bus.send(msg));
    EXPECT_FALSE(bus.send(msg));
    EXPECT_EQ(received.load(), 1);
}

TEST(MessageBusTest, RateLimitRejectsExcessMessages) {
    MessageBus bus({1, std::chrono::minutes(1)});
    std::atomic<int> received = 0;

    bus.subscribe("executor", [&](const Message&) { ++received; });

    Message msg1;
    msg1.performative = Performative::REQUEST;
    msg1.sender = "planner";
    msg1.receivers = {"executor"};

    Message msg2 = msg1;
    msg2.id = evoclaw::generate_uuid();

    EXPECT_TRUE(bus.send(msg1));
    EXPECT_FALSE(bus.send(msg2));
    EXPECT_EQ(received.load(), 1);
}

TEST(MessageBusTest, BroadcastSendsToAllSubscribers) {
    MessageBus bus;
    std::atomic<int> received = 0;

    bus.subscribe("a1", [&](const Message&) { ++received; });
    bus.subscribe("a2", [&](const Message&) { ++received; });

    Message msg;
    msg.performative = Performative::INFORM;
    msg.sender = "router";

    const std::size_t delivered = bus.broadcast(msg);
    EXPECT_EQ(delivered, 2U);
    EXPECT_EQ(received.load(), 2);
}

} // namespace
