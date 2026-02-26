#include <gtest/gtest.h>
#include "protocol/basic_rule_engine.h"

using namespace evoclaw;

class BasicRuleEngineTest : public ::testing::Test {
 protected:
  BasicRuleEngine engine;
};

TEST_F(BasicRuleEngineTest, EmptyConstraints) {
  nlohmann::json payload = {{"name", "test"}};
  nlohmann::json constraints = nlohmann::json::array();
  auto result = engine.Validate(payload, constraints);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

TEST_F(BasicRuleEngineTest, EqualityCheck) {
  nlohmann::json payload = {{"status", "active"}};
  nlohmann::json constraints = {{{"field", "status"}, {"op", "eq"}, {"value", "active"}}};
  auto result = engine.Validate(payload, constraints);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

TEST_F(BasicRuleEngineTest, EqualityCheckFail) {
  nlohmann::json payload = {{"status", "inactive"}};
  nlohmann::json constraints = {{{"field", "status"}, {"op", "eq"}, {"value", "active"}}};
  auto result = engine.Validate(payload, constraints);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
}

TEST_F(BasicRuleEngineTest, RangeCheck) {
  nlohmann::json payload = {{"score", 75}};
  nlohmann::json constraints = {{{"field", "score"}, {"op", "range"}, {"min", 0}, {"max", 100}}};
  auto result = engine.Validate(payload, constraints);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

TEST_F(BasicRuleEngineTest, RangeCheckFail) {
  nlohmann::json payload = {{"score", 150}};
  nlohmann::json constraints = {{{"field", "score"}, {"op", "range"}, {"min", 0}, {"max", 100}}};
  auto result = engine.Validate(payload, constraints);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
}

TEST_F(BasicRuleEngineTest, InCheck) {
  nlohmann::json payload = {{"role", "planner"}};
  nlohmann::json constraints = {{{"field", "role"}, {"op", "in"}, {"values", {"planner", "executor", "critic"}}}};
  auto result = engine.Validate(payload, constraints);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}

TEST_F(BasicRuleEngineTest, RequiredFieldMissing) {
  nlohmann::json payload = {{"name", "test"}};
  nlohmann::json constraints = {{{"field", "priority"}, {"op", "gte"}, {"value", 0}, {"required", true}}};
  auto result = engine.Validate(payload, constraints);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->passed);
}

TEST_F(BasicRuleEngineTest, GreaterThan) {
  nlohmann::json payload = {{"tokens", 500}};
  nlohmann::json constraints = {{{"field", "tokens"}, {"op", "gt"}, {"value", 0}}};
  auto result = engine.Validate(payload, constraints);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->passed);
}
