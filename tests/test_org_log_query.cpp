#include "memory/org_log.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>

namespace {

class OrgLogQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_org_query_" + evoclaw::generate_uuid());
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    static evoclaw::Timestamp at_seconds(const int seconds) {
        return evoclaw::Timestamp(std::chrono::seconds(seconds));
    }

    std::filesystem::path test_dir_;
};

TEST_F(OrgLogQueryTest, QueryByTimeRange) {
    evoclaw::memory::OrgLog log(test_dir_);

    evoclaw::memory::OrgLogEntry e1;
    e1.entry_id = "e1";
    e1.agent_id = "agent-a";
    e1.task_id = "t1";
    e1.timestamp = at_seconds(1000);
    log.append(e1);

    evoclaw::memory::OrgLogEntry e2;
    e2.entry_id = "e2";
    e2.agent_id = "agent-b";
    e2.task_id = "t2";
    e2.timestamp = at_seconds(1010);
    log.append(e2);

    evoclaw::memory::OrgLogEntry e3;
    e3.entry_id = "e3";
    e3.agent_id = "agent-c";
    e3.task_id = "t3";
    e3.timestamp = at_seconds(1020);
    log.append(e3);

    const auto results = log.query_by_time_range(at_seconds(1005), at_seconds(1015));
    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results[0].entry_id, "e2");
}

TEST_F(OrgLogQueryTest, QueryByAgentAndTime) {
    evoclaw::memory::OrgLog log(test_dir_);

    evoclaw::memory::OrgLogEntry e1;
    e1.entry_id = "a1";
    e1.agent_id = "agent-a";
    e1.task_id = "t1";
    e1.timestamp = at_seconds(1000);
    log.append(e1);

    evoclaw::memory::OrgLogEntry e2;
    e2.entry_id = "a2";
    e2.agent_id = "agent-a";
    e2.task_id = "t2";
    e2.timestamp = at_seconds(1010);
    log.append(e2);

    evoclaw::memory::OrgLogEntry e3;
    e3.entry_id = "b1";
    e3.agent_id = "agent-b";
    e3.task_id = "t3";
    e3.timestamp = at_seconds(1012);
    log.append(e3);

    const auto results = log.query_by_agent_and_time("agent-a", at_seconds(1005), at_seconds(1015));
    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results[0].entry_id, "a2");
}

TEST_F(OrgLogQueryTest, GetStatsForRange) {
    evoclaw::memory::OrgLog log(test_dir_);

    evoclaw::memory::OrgLogEntry e1;
    e1.entry_id = "s1";
    e1.agent_id = "agent-a";
    e1.timestamp = at_seconds(1000);
    e1.duration_ms = 10.0;
    e1.critic_score = 0.8;
    e1.metadata = {{"success", true}};
    log.append(e1);

    evoclaw::memory::OrgLogEntry e2;
    e2.entry_id = "s2";
    e2.agent_id = "agent-a";
    e2.timestamp = at_seconds(1010);
    e2.duration_ms = 20.0;
    e2.critic_score = 0.4;
    e2.metadata = {{"success", false}};
    log.append(e2);

    evoclaw::memory::OrgLogEntry e3;
    e3.entry_id = "s3";
    e3.agent_id = "agent-b";
    e3.timestamp = at_seconds(1020);
    e3.duration_ms = 30.0;
    e3.critic_score = 0.6;
    e3.metadata = {{"success", true}};
    log.append(e3);

    const auto stats = log.get_stats_for_range(at_seconds(999), at_seconds(1021));
    EXPECT_EQ(stats.total_tasks, 3);
    EXPECT_EQ(stats.successful_tasks, 2);
    EXPECT_NEAR(stats.avg_duration_ms, 20.0, 1e-9);
    EXPECT_NEAR(stats.avg_critic_score, 0.6, 1e-9);
    EXPECT_EQ(stats.tasks_by_agent.at("agent-a"), 2);
    EXPECT_EQ(stats.tasks_by_agent.at("agent-b"), 1);
}

} // namespace
