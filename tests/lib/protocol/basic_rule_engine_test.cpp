#include <gtest/gtest.h>

#include "protocol/basic_rule_engine.h"

namespace evoclaw::protocol {
namespace {

class BasicRuleEngineTest : public ::testing::Test {
 protected:
  BasicRuleEngine engine_;
};

// ── required 约束 ─────────────────────────────────────────

TEST_F(BasicRuleEngineTest, Validate_RequiredFieldPresent_Passes) {
  nlohmann::json payload = {{"name", "test"}};
  nlohmann::json rules = {{"name", {{"required", true}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
  EXPECT_TRUE(result->violations.empty());
}

TEST_F(BasicRuleEngineTest, Validate_RequiredFieldMissing_Fails) {
  nlohmann::json payload = {{"other", "value"}};
  nlohmann::json rules = {{"name", {{"required", true}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
  EXPECT_EQ(result->violations.size(), 1);
}

// ── type 约束 ─────────────────────────────────────────────

TEST_F(BasicRuleEngineTest, Validate_CorrectType_Passes) {
  nlohmann::json payload = {{"count", 42}};
  nlohmann::json rules = {{"count", {{"type", "number"}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

TEST_F(BasicRuleEngineTest, Validate_WrongType_Fails) {
  nlohmann::json payload = {{"count", "not_a_number"}};
  nlohmann::json rules = {{"count", {{"type", "number"}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
}

// ── min / max 约束 ────────────────────────────────────────

TEST_F(BasicRuleEngineTest, Validate_NumberInRange_Passes) {
  nlohmann::json payload = {{"score", 75}};
  nlohmann::json rules = {{"score", {{"type", "number"}, {"min", 0}, {"max", 100}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

TEST_F(BasicRuleEngineTest, Validate_NumberBelowMin_Fails) {
  nlohmann::json payload = {{"score", -1}};
  nlohmann::json rules = {{"score", {{"type", "number"}, {"min", 0}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
}

TEST_F(BasicRuleEngineTest, Validate_NumberAboveMax_Fails) {
  nlohmann::json payload = {{"score", 101}};
  nlohmann::json rules = {{"score", {{"type", "number"}, {"max", 100}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
}

// ── min_length / max_length 约束 ──────────────────────────

TEST_F(BasicRuleEngineTest, Validate_StringLengthInRange_Passes) {
  nlohmann::json payload = {{"name", "hello"}};
  nlohmann::json rules = {{"name", {{"type", "string"}, {"min_length", 1}, {"max_length", 10}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

TEST_F(BasicRuleEngineTest, Validate_StringTooShort_Fails) {
  nlohmann::json payload = {{"name", ""}};
  nlohmann::json rules = {{"name", {{"type", "string"}, {"min_length", 1}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
}

// ── enum 约束 ─────────────────────────────────────────────

TEST_F(BasicRuleEngineTest, Validate_ValueInEnum_Passes) {
  nlohmann::json payload = {{"status", "active"}};
  nlohmann::json rules = {{"status", {{"enum", {"active", "inactive", "pending"}}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

TEST_F(BasicRuleEngineTest, Validate_ValueNotInEnum_Fails) {
  nlohmann::json payload = {{"status", "unknown"}};
  nlohmann::json rules = {{"status", {{"enum", {"active", "inactive"}}}}};

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
}

// ── 多字段组合 ────────────────────────────────────────────

TEST_F(BasicRuleEngineTest, Validate_MultipleFields_AllPass) {
  nlohmann::json payload = {
      {"name", "task-001"},
      {"priority", 3},
      {"active", true},
  };
  nlohmann::json rules = {
      {"name", {{"required", true}, {"type", "string"}, {"min_length", 1}}},
      {"priority", {{"required", true}, {"type", "number"}, {"min", 1}, {"max", 5}}},
      {"active", {{"type", "boolean"}}},
  };

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

TEST_F(BasicRuleEngineTest, Validate_MultipleViolations_ReportsAll) {
  nlohmann::json payload = {{"priority", 999}};
  nlohmann::json rules = {
      {"name", {{"required", true}}},
      {"priority", {{"type", "number"}, {"max", 5}}},
  };

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
  EXPECT_EQ(result->violations.size(), 2);  // name missing + priority > max
}

// ── 错误处理 ──────────────────────────────────────────────

TEST_F(BasicRuleEngineTest, Validate_InvalidRulesFormat_ReturnsError) {
  nlohmann::json payload = {{"name", "test"}};
  nlohmann::json rules = "not_an_object";

  auto result = engine_.Validate(payload, rules);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ErrorCode::kInvalidArg);
}

// ── 可选字段不存在时跳过 ──────────────────────────────────

TEST_F(BasicRuleEngineTest, Validate_OptionalFieldMissing_Passes) {
  nlohmann::json payload = {{"name", "test"}};
  nlohmann::json rules = {
      {"name", {{"required", true}}},
      {"description", {{"type", "string"}}},  // 可选
  };

  auto result = engine_.Validate(payload, rules);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

}  // namespace
}  // namespace evoclaw::protocol
