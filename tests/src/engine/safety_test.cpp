#include <gtest/gtest.h>
#include "safety/risk_classifier.h"
#include "safety/permission_manager.h"

using namespace evoclaw;

// ── RiskClassifier 测试 ──

TEST(RiskClassifierTest, ReadIsL0) {
  RiskClassifier classifier;
  Message msg;
  msg.msg_type = "file.read";

  EXPECT_EQ(classifier.Classify(msg), 0);
}

TEST(RiskClassifierTest, WriteIsL1) {
  RiskClassifier classifier;
  Message msg;
  msg.msg_type = "file.write";

  EXPECT_EQ(classifier.Classify(msg), 1);
}

TEST(RiskClassifierTest, DeleteIsL2) {
  RiskClassifier classifier;
  Message msg;
  msg.msg_type = "file.delete";

  EXPECT_EQ(classifier.Classify(msg), 2);
}

TEST(RiskClassifierTest, NetworkIsL3) {
  RiskClassifier classifier;
  Message msg;
  msg.msg_type = "network.request";

  EXPECT_EQ(classifier.Classify(msg), 3);
}

TEST(RiskClassifierTest, CustomRisk) {
  RiskClassifier classifier;
  classifier.RegisterRisk("custom.action", 2);

  Message msg;
  msg.msg_type = "custom.action";

  EXPECT_EQ(classifier.Classify(msg), 2);
}

TEST(RiskClassifierTest, PayloadRiskLevel) {
  RiskClassifier classifier;
  Message msg;
  msg.msg_type = "unknown.action";
  msg.payload = {{"risk_level", 2}};

  EXPECT_EQ(classifier.Classify(msg), 2);
}

TEST(RiskClassifierTest, DefaultL0ForUnknown) {
  RiskClassifier classifier;
  Message msg;
  msg.msg_type = "something_unknown";

  EXPECT_EQ(classifier.Classify(msg), 0);
}

// ── PermissionManager 测试 ──

TEST(PermissionManagerTest, GrantAndCheck) {
  PermissionManager pm;
  pm.GrantPermission("agent_1", "file.read");
  pm.GrantPermission("agent_1", "file.write");

  EXPECT_TRUE(pm.HasPermission("agent_1", "file.read"));
  EXPECT_TRUE(pm.HasPermission("agent_1", "file.write"));
  EXPECT_FALSE(pm.HasPermission("agent_1", "shell.execute"));
}

TEST(PermissionManagerTest, RevokePermission) {
  PermissionManager pm;
  pm.GrantPermission("agent_1", "file.read");
  pm.RevokePermission("agent_1", "file.read");

  EXPECT_FALSE(pm.HasPermission("agent_1", "file.read"));
}

TEST(PermissionManagerTest, SuperAgent) {
  PermissionManager pm;
  pm.GrantAll("admin");

  EXPECT_TRUE(pm.HasPermission("admin", "anything"));
  EXPECT_TRUE(pm.HasPermission("admin", "shell.execute"));
  EXPECT_TRUE(pm.HasAllPermissions("admin"));
}

TEST(PermissionManagerTest, NoPermissionByDefault) {
  PermissionManager pm;
  EXPECT_FALSE(pm.HasPermission("unknown_agent", "file.read"));
}

TEST(PermissionManagerTest, GetPermissions) {
  PermissionManager pm;
  pm.GrantPermission("agent_1", "file.read");
  pm.GrantPermission("agent_1", "file.write");

  auto perms = pm.GetPermissions("agent_1");
  EXPECT_EQ(perms.size(), 2u);
  EXPECT_TRUE(perms.contains("file.read"));
  EXPECT_TRUE(perms.contains("file.write"));
}
