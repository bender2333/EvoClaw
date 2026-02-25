#include "zone/zone_manager.hpp"

#include <gtest/gtest.h>

namespace {

TEST(ZoneManagerTest, AssignAndReadPolicy) {
    evoclaw::zone::ZoneManager manager;

    manager.assign_zone("agent-stable", evoclaw::zone::Zone::STABLE);
    manager.assign_zone("agent-exp", evoclaw::zone::Zone::EXPLORATION);

    EXPECT_EQ(manager.get_zone("agent-stable"), evoclaw::zone::Zone::STABLE);
    EXPECT_EQ(manager.get_zone("agent-exp"), evoclaw::zone::Zone::EXPLORATION);

    const auto stable_policy = manager.get_policy("agent-stable");
    EXPECT_DOUBLE_EQ(stable_policy.token_budget_ratio, 1.0);
    EXPECT_TRUE(stable_policy.can_face_user);

    const auto exploration_policy = manager.get_policy("agent-exp");
    EXPECT_DOUBLE_EQ(exploration_policy.token_budget_ratio, 0.7);
    EXPECT_FALSE(exploration_policy.can_face_user);
}

TEST(ZoneManagerTest, PromotionEligibilityAndPromote) {
    evoclaw::zone::ZoneManager manager;
    manager.assign_zone("agent-exp", evoclaw::zone::Zone::EXPLORATION);

    for (int i = 0; i < 20; ++i) {
        manager.record_result("agent-exp", true);
    }
    for (int i = 0; i < 5; ++i) {
        manager.record_result("agent-exp", false);
    }

    EXPECT_TRUE(manager.is_promotion_eligible("agent-exp"));
    manager.promote_to_stable("agent-exp");
    EXPECT_EQ(manager.get_zone("agent-exp"), evoclaw::zone::Zone::STABLE);
    EXPECT_FALSE(manager.is_promotion_eligible("agent-exp"));
}

TEST(ZoneManagerTest, StatusContainsSummaryAndMetrics) {
    evoclaw::zone::ZoneManager manager;
    manager.assign_zone("agent-a", evoclaw::zone::Zone::STABLE);
    manager.assign_zone("agent-b", evoclaw::zone::Zone::EXPLORATION);
    manager.record_result("agent-b", true);
    manager.record_result("agent-b", false);

    const auto status = manager.status();
    ASSERT_TRUE(status.contains("summary"));
    ASSERT_TRUE(status.contains("agents"));
    EXPECT_EQ(status["summary"].value("stable", 0), 1);
    EXPECT_EQ(status["summary"].value("exploration", 0), 1);
    ASSERT_TRUE(status["agents"].is_array());
    EXPECT_EQ(status["agents"].size(), 2);
}

} // namespace
