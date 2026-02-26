#include <gtest/gtest.h>
#include "router/capability_matrix.h"
#include "router/router.h"

using namespace evoclaw;

// ── CapabilityMatrix 测试 ──

TEST(CapabilityMatrixTest, RegisterAndGetBest) {
  CapabilityMatrix matrix;
  matrix.RegisterModule("mod_a", {{"task", 0.8}, {"search", 0.6}});
  matrix.RegisterModule("mod_b", {{"task", 0.9}, {"search", 0.7}});

  auto best = matrix.GetBestModule("task");
  ASSERT_TRUE(best.has_value());
  EXPECT_EQ(best->module_id, "mod_b");
  EXPECT_DOUBLE_EQ(best->score, 0.9);
}

TEST(CapabilityMatrixTest, GetBestNotFound) {
  CapabilityMatrix matrix;
  auto best = matrix.GetBestModule("nonexistent");
  ASSERT_FALSE(best.has_value());
}

TEST(CapabilityMatrixTest, UpdateScore) {
  CapabilityMatrix matrix;
  matrix.RegisterModule("mod_a", {{"task", 0.5}});
  matrix.UpdateScore("mod_a", "task", 0.95);

  auto best = matrix.GetBestModule("task");
  ASSERT_TRUE(best.has_value());
  EXPECT_DOUBLE_EQ(best->score, 0.95);
}

TEST(CapabilityMatrixTest, GetAllScoresSorted) {
  CapabilityMatrix matrix;
  matrix.RegisterModule("mod_a", {{"task", 0.3}});
  matrix.RegisterModule("mod_b", {{"task", 0.9}});
  matrix.RegisterModule("mod_c", {{"task", 0.6}});

  auto all = matrix.GetAllScores("task");
  ASSERT_EQ(all.size(), 3u);
  EXPECT_EQ(all[0].module_id, "mod_b");  // 最高
  EXPECT_EQ(all[2].module_id, "mod_a");  // 最低
}

TEST(CapabilityMatrixTest, RemoveModule) {
  CapabilityMatrix matrix;
  matrix.RegisterModule("mod_a", {{"task", 0.8}});
  EXPECT_TRUE(matrix.HasModule("mod_a"));

  matrix.RemoveModule("mod_a");
  EXPECT_FALSE(matrix.HasModule("mod_a"));
}

// ── Router 测试 ──

TEST(RouterTest, RouteSelectsBest) {
  CapabilityMatrix matrix;
  matrix.RegisterModule("mod_a", {{"task", 0.3}});
  matrix.RegisterModule("mod_b", {{"task", 0.9}});

  // epsilon=0 → 总是选最佳
  Router router(matrix, 0.0);

  Message msg;
  msg.msg_type = "task.request";

  auto result = router.Route(msg);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "mod_b");
}

TEST(RouterTest, RouteEmptyMsgType) {
  CapabilityMatrix matrix;
  Router router(matrix, 0.0);

  Message msg;
  msg.msg_type = "";

  auto result = router.Route(msg);
  ASSERT_FALSE(result.has_value());
}

TEST(RouterTest, RouteNoModules) {
  CapabilityMatrix matrix;
  Router router(matrix, 0.0);

  Message msg;
  msg.msg_type = "task.request";

  auto result = router.Route(msg);
  ASSERT_FALSE(result.has_value());
}

TEST(RouterTest, EpsilonExploration) {
  CapabilityMatrix matrix;
  matrix.RegisterModule("mod_a", {{"task", 0.9}});
  matrix.RegisterModule("mod_b", {{"task", 0.1}});

  // epsilon=1.0 → 总是探索（选非最佳）
  Router router(matrix, 1.0);

  Message msg;
  msg.msg_type = "task.request";

  // 多次路由，应该至少有一次选到 mod_b
  bool found_b = false;
  for (int i = 0; i < 100; ++i) {
    auto result = router.Route(msg);
    ASSERT_TRUE(result.has_value());
    if (*result == "mod_b") found_b = true;
  }
  EXPECT_TRUE(found_b);
}

TEST(RouterTest, ExtractCapabilityFromDotNotation) {
  CapabilityMatrix matrix;
  matrix.RegisterModule("mod_a", {{"search", 0.8}});

  Router router(matrix, 0.0);

  Message msg;
  msg.msg_type = "search.query.deep";

  auto result = router.Route(msg);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "mod_a");
}

TEST(RouterTest, SingleModuleAlwaysSelected) {
  CapabilityMatrix matrix;
  matrix.RegisterModule("only_one", {{"task", 0.5}});

  // 即使 epsilon=1.0，只有一个模块时也只能选它
  Router router(matrix, 1.0);

  Message msg;
  msg.msg_type = "task.request";

  for (int i = 0; i < 10; ++i) {
    auto result = router.Route(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "only_one");
  }
}
