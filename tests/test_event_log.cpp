#include "event_log/event_log.hpp"

#include <gtest/gtest.h>
#include <filesystem>

namespace {

class EventLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_path_ = std::filesystem::temp_directory_path() / ("evoclaw_elog_" + evoclaw::generate_uuid() + ".jsonl");
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(test_path_, ec);
    }

    std::filesystem::path test_path_;
};

TEST_F(EventLogTest, AppendAndQuery) {
    evoclaw::event_log::EventLog log(test_path_);

    evoclaw::event_log::Event e1;
    e1.type = evoclaw::event_log::EventType::TASK_COMPLETE;
    e1.actor = "agent-1";
    e1.target = "task-1";
    e1.action = "completed";
    e1.details = {{"score", 0.9}};
    log.append(e1);

    evoclaw::event_log::Event e2;
    e2.type = evoclaw::event_log::EventType::EVOLUTION;
    e2.actor = "evolver";
    e2.target = "agent-1";
    e2.action = "prompt_mutation";
    e2.details = {{"improvement", 0.15}};
    log.append(e2);

    auto all = log.query({});
    EXPECT_EQ(all.size(), 2);
}

TEST_F(EventLogTest, HashChainIntegrity) {
    evoclaw::event_log::EventLog log(test_path_);

    for (int i = 0; i < 5; ++i) {
        evoclaw::event_log::Event e;
        e.type = evoclaw::event_log::EventType::TASK_COMPLETE;
        e.actor = "agent-" + std::to_string(i);
        e.target = "task-" + std::to_string(i);
        e.action = "completed";
        e.details = {{"iteration", i}};
        log.append(e);
    }

    EXPECT_TRUE(log.verify_integrity());
}

TEST_F(EventLogTest, EmptyLogIntegrity) {
    evoclaw::event_log::EventLog log(test_path_);
    EXPECT_TRUE(log.verify_integrity());
}

} // namespace
