#include "memory/working_memory.hpp"
#include "memory/org_log.hpp"

#include <gtest/gtest.h>
#include <filesystem>

namespace {

TEST(WorkingMemoryTest, StoreAndRetrieve) {
    evoclaw::memory::WorkingMemory wm;
    wm.store("key1", "value1");
    wm.store("key2", 42);

    auto v1 = wm.retrieve("key1");
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(v1->get<std::string>(), "value1");

    auto v2 = wm.retrieve("key2");
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(v2->get<int>(), 42);

    auto v3 = wm.retrieve("nonexistent");
    EXPECT_FALSE(v3.has_value());
}

TEST(WorkingMemoryTest, ClearRemovesAll) {
    evoclaw::memory::WorkingMemory wm;
    wm.store("a", 1);
    wm.store("b", 2);
    wm.clear();

    EXPECT_FALSE(wm.retrieve("a").has_value());
    EXPECT_FALSE(wm.retrieve("b").has_value());
}

TEST(WorkingMemoryTest, Summaries) {
    evoclaw::memory::WorkingMemory wm;
    wm.store("task", "build system");
    wm.store("status", "in progress");

    auto one_line = wm.one_line_summary();
    EXPECT_FALSE(one_line.empty());

    auto paragraph = wm.paragraph_summary();
    EXPECT_FALSE(paragraph.empty());
}

class OrgLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_test_" + evoclaw::generate_uuid());
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::filesystem::path test_dir_;
};

TEST_F(OrgLogTest, AppendAndQueryByAgent) {
    evoclaw::memory::OrgLog log(test_dir_);

    evoclaw::memory::OrgLogEntry e1;
    e1.entry_id = "e1";
    e1.agent_id = "agent-a";
    e1.task_id = "t1";
    e1.critic_score = 0.8;
    log.append(e1);

    evoclaw::memory::OrgLogEntry e2;
    e2.entry_id = "e2";
    e2.agent_id = "agent-b";
    e2.task_id = "t2";
    e2.critic_score = 0.6;
    log.append(e2);

    auto results = log.query_by_agent("agent-a");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].entry_id, "e1");

    results = log.query_by_agent("agent-b");
    EXPECT_EQ(results.size(), 1);
}

TEST_F(OrgLogTest, AverageScore) {
    evoclaw::memory::OrgLog log(test_dir_);

    for (int i = 0; i < 5; ++i) {
        evoclaw::memory::OrgLogEntry e;
        e.agent_id = "agent-x";
        e.critic_score = 0.2 * (i + 1);  // 0.2, 0.4, 0.6, 0.8, 1.0
        log.append(e);
    }

    double avg = log.average_score("agent-x");
    EXPECT_NEAR(avg, 0.6, 0.01);
}

TEST_F(OrgLogTest, AllEntries) {
    evoclaw::memory::OrgLog log(test_dir_);

    for (int i = 0; i < 3; ++i) {
        evoclaw::memory::OrgLogEntry e;
        e.agent_id = "agent-" + std::to_string(i);
        e.critic_score = 0.5;
        log.append(e);
    }

    auto all = log.all_entries();
    EXPECT_EQ(all.size(), 3);
}

} // namespace
