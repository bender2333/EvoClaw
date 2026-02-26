#include <gtest/gtest.h>
#include "protocol/cerebellum.h"
#include "protocol/basic_rule_engine.h"

using namespace evoclaw;

TEST(CerebellumTest, PreProcessFillsIdAndTimestamp) {
  auto rule_engine = std::make_shared<BasicRuleEngine>();
  Cerebellum cerebellum(rule_engine);

  Message msg;
  msg.msg_type = "task.request";
  msg.source = "engine";
  msg.payload = {{"task", "test"}};

  auto result = cerebellum.PreProcess(std::move(msg));
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->msg_id.empty());
  EXPECT_FALSE(result->timestamp.empty());
  EXPECT_EQ(result->msg_type, "task.request");
}

TEST(CerebellumTest, PreProcessValidatesConstraints) {
  auto rule_engine = std::make_shared<BasicRuleEngine>();
  Cerebellum cerebellum(rule_engine);

  Message msg;
  msg.msg_type = "task.request";
  msg.source = "engine";
  msg.payload = {
      {"priority", -1},
      {"_constraints", {{{"field", "priority"}, {"op", "gte"}, {"value", 0}}}}
  };

  auto result = cerebellum.PreProcess(std::move(msg));
  ASSERT_FALSE(result.has_value());
}

TEST(CerebellumTest, PostProcessDeserializesAndValidates) {
  auto rule_engine = std::make_shared<BasicRuleEngine>();
  Cerebellum cerebellum(rule_engine);

  nlohmann::json j;
  j["msg_id"] = "test-id";
  j["msg_type"] = "task.response";
  j["source"] = "compiler";
  j["target"] = "engine";
  j["timestamp"] = "2026-02-26T10:30:00Z";
  j["payload"] = {{"result", "ok"}};

  auto result = cerebellum.PostProcess(j.dump());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->msg_id, "test-id");
}
