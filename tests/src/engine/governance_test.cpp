#include <gtest/gtest.h>
#include "governance/governance_gate.h"
#include "governance/constitution.h"
#include "governance/entropy_monitor.h"

using namespace evoclaw;

// ── GovernanceGate 测试 ──

TEST(GovernanceGateTest, AutoApproveL0) {
  GovernanceGate gate;
  Message msg;
  msg.msg_id = "msg_001";
  msg.msg_type = "file.read";

  auto result = gate.Approve(msg, 0);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(*result);  // 自动通过
  EXPECT_EQ(gate.GetPendingApprovals().size(), 0u);
}

TEST(GovernanceGateTest, AutoApproveL2WithLog) {
  GovernanceGate gate;
  Message msg;
  msg.msg_id = "msg_002";
  msg.msg_type = "file.delete";

  auto result = gate.Approve(msg, 2);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(*result);
  EXPECT_EQ(gate.GetAuditLogSize(), 1u);
}

TEST(GovernanceGateTest, PendingApprovalL3) {
  GovernanceGate gate;
  Message msg;
  msg.msg_id = "msg_003";
  msg.msg_type = "email.send";

  auto result = gate.Approve(msg, 3);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(*result);  // 需要用户确认

  auto pending = gate.GetPendingApprovals();
  EXPECT_EQ(pending.size(), 1u);
  EXPECT_EQ(pending[0].msg_id, "msg_003");
}

TEST(GovernanceGateTest, UserApproveAndReject) {
  GovernanceGate gate;

  Message msg1;
  msg1.msg_id = "msg_a";
  msg1.msg_type = "network.request";
  gate.Approve(msg1, 3);

  Message msg2;
  msg2.msg_id = "msg_b";
  msg2.msg_type = "publish.release";
  gate.Approve(msg2, 4);

  EXPECT_EQ(gate.GetPendingApprovals().size(), 2u);

  auto approve_result = gate.UserApprove("msg_a");
  ASSERT_TRUE(approve_result.has_value());

  auto reject_result = gate.UserReject("msg_b");
  ASSERT_TRUE(reject_result.has_value());

  EXPECT_EQ(gate.GetPendingApprovals().size(), 0u);

  auto status_a = gate.GetStatus("msg_a");
  ASSERT_TRUE(status_a.has_value());
  EXPECT_EQ(*status_a, ApprovalStatus::kApproved);

  auto status_b = gate.GetStatus("msg_b");
  ASSERT_TRUE(status_b.has_value());
  EXPECT_EQ(*status_b, ApprovalStatus::kRejected);
}

TEST(GovernanceGateTest, ApproveNonexistent) {
  GovernanceGate gate;
  auto result = gate.UserApprove("nonexistent");
  ASSERT_FALSE(result.has_value());
}

// ── Constitution 测试 ──

TEST(ConstitutionTest, CheckCompliant) {
  Constitution constitution;
  constitution.AddRule({"r1", "禁止自动发送邮件", "email.send"});

  Message msg;
  msg.msg_type = "file.read";

  auto result = constitution.Check(msg);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(*result);  // 合规
}

TEST(ConstitutionTest, CheckViolation) {
  Constitution constitution;
  constitution.AddRule({"r1", "禁止自动发送邮件", "email.send"});

  Message msg;
  msg.msg_type = "email.send.batch";

  auto result = constitution.Check(msg);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(*result);  // 违反宪法
}

TEST(ConstitutionTest, AmendmentCooldownPeriod) {
  Constitution constitution;
  ConstitutionRule rule{"r_new", "新规则", "dangerous.action"};

  auto id_result = constitution.ProposeAmendment(rule);
  ASSERT_TRUE(id_result.has_value());

  // 冷静期未过，确认应该失败
  auto confirm_result = constitution.ConfirmAmendment(*id_result);
  ASSERT_TRUE(confirm_result.has_value());
  EXPECT_FALSE(*confirm_result);  // 冷静期未过
}

TEST(ConstitutionTest, AmendmentNotFound) {
  Constitution constitution;
  auto result = constitution.ConfirmAmendment("nonexistent");
  ASSERT_FALSE(result.has_value());
}

// ── EntropyMonitor 测试 ──

TEST(EntropyMonitorTest, InitDefaults) {
  EntropyMonitor monitor;
  monitor.InitDefaults();

  // 初始值应该都是 0，不超阈值
  auto alerts = monitor.Check();
  EXPECT_TRUE(alerts.empty());
}

TEST(EntropyMonitorTest, AlertOnThresholdExceeded) {
  EntropyMonitor monitor;
  monitor.SetThreshold("task_rounds", 10.0);
  monitor.Update("task_rounds", 15.0);

  auto alerts = monitor.Check();
  ASSERT_EQ(alerts.size(), 1u);
  EXPECT_EQ(alerts[0].metric_name, "task_rounds");
  EXPECT_DOUBLE_EQ(alerts[0].current_value, 15.0);
  EXPECT_DOUBLE_EQ(alerts[0].threshold, 10.0);
}

TEST(EntropyMonitorTest, NoAlertBelowThreshold) {
  EntropyMonitor monitor;
  monitor.SetThreshold("module_density", 0.7);
  monitor.Update("module_density", 0.5);

  auto alerts = monitor.Check();
  EXPECT_TRUE(alerts.empty());
}

TEST(EntropyMonitorTest, BatchUpdate) {
  EntropyMonitor monitor;
  monitor.SetThreshold("a", 5.0);
  monitor.SetThreshold("b", 5.0);

  monitor.UpdateAll({{"a", 10.0}, {"b", 3.0}});

  auto alerts = monitor.Check();
  ASSERT_EQ(alerts.size(), 1u);
  EXPECT_EQ(alerts[0].metric_name, "a");
}
