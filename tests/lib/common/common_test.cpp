#include <gtest/gtest.h>

#include "common/error.h"

namespace evoclaw {
namespace {

// Story 1.1 验收：GTest 空测试用例能通过 ctest
TEST(CommonTest, ErrorMake_ValidArgs_ReturnsError) {
  auto err = Error::Make(ErrorCode::kNotFound, "not found", "test");
  EXPECT_EQ(err.code, ErrorCode::kNotFound);
  EXPECT_EQ(err.message, "not found");
  EXPECT_EQ(err.context, "test");
}

TEST(CommonTest, Result_Success_HasValue) {
  Result<int> result = 42;
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 42);
}

TEST(CommonTest, Result_Error_HasError) {
  Result<int> result =
      std::unexpected(Error::Make(ErrorCode::kInternal, "fail"));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ErrorCode::kInternal);
}

TEST(CommonTest, Status_Success_HasValue) {
  Status status{};
  EXPECT_TRUE(status.has_value());
}

}  // namespace
}  // namespace evoclaw
