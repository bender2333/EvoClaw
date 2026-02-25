#include "budget/budget_tracker.hpp"

#include <gtest/gtest.h>

namespace {

TEST(BudgetTrackerTest, RecordAndQuery) {
    evoclaw::budget::BudgetTracker tracker;

    tracker.record("agent-a", 100, 50);
    tracker.record("agent-a", 200, 100);
    tracker.record("agent-b", 50, 25);

    EXPECT_EQ(tracker.agent_tokens("agent-a"), 450);  // 100+50+200+100
    EXPECT_EQ(tracker.agent_tokens("agent-b"), 75);
    EXPECT_EQ(tracker.total_tokens(), 525);
    EXPECT_EQ(tracker.agent_tokens("nonexistent"), 0);
}

TEST(BudgetTrackerTest, GlobalLimit) {
    evoclaw::budget::BudgetTracker tracker;
    tracker.set_global_limit(500);

    tracker.record("agent-a", 200, 100);
    EXPECT_FALSE(tracker.is_global_budget_exceeded());

    tracker.record("agent-b", 150, 100);
    EXPECT_TRUE(tracker.is_global_budget_exceeded());  // 550 > 500
}

TEST(BudgetTrackerTest, NoLimitMeansUnlimited) {
    evoclaw::budget::BudgetTracker tracker;
    // global_limit_ = 0 means unlimited
    tracker.record("agent-a", 999999, 999999);
    EXPECT_FALSE(tracker.is_global_budget_exceeded());
}

TEST(BudgetTrackerTest, Report) {
    evoclaw::budget::BudgetTracker tracker;
    tracker.record("agent-a", 100, 50);

    auto report = tracker.report();
    EXPECT_TRUE(report.contains("total_tokens"));
    EXPECT_TRUE(report.contains("agents"));
    EXPECT_EQ(report["total_tokens"].get<int>(), 150);
}

} // namespace
