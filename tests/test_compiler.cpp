#include "compiler/compiler.hpp"
#include "memory/org_log.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace {

class CompilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_compiler_" + evoclaw::generate_uuid());
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    static void append_step(evoclaw::memory::OrgLog& log,
                            const std::string& task_id,
                            const std::string& agent_id,
                            const std::string& primitive_id,
                            const bool success) {
        evoclaw::memory::OrgLogEntry entry;
        entry.task_id = task_id;
        entry.agent_id = agent_id;
        entry.user_feedback_positive = success;
        entry.metadata = {
            {"primitive_id", primitive_id},
            {"success", success}
        };
        log.append(entry);
    }

    std::filesystem::path test_dir_;
};

TEST_F(CompilerTest, DetectPatternsFromRepeatedTaskSequences) {
    evoclaw::memory::OrgLog log(test_dir_);

    append_step(log, "task-1", "planner-1", "primitive.plan", true);
    append_step(log, "task-1", "executor-1", "primitive.execute", true);
    append_step(log, "task-2", "planner-1", "primitive.plan", true);
    append_step(log, "task-2", "executor-1", "primitive.execute", true);
    append_step(log, "task-3", "planner-1", "primitive.plan", true);
    append_step(log, "task-3", "executor-1", "primitive.execute", false);

    evoclaw::compiler::Compiler compiler(log);
    const auto patterns = compiler.detect_patterns(3);

    ASSERT_EQ(patterns.size(), 1);
    EXPECT_EQ(patterns[0].observation_count, 3);
    EXPECT_EQ(patterns[0].step_sequence.size(), 2);
    EXPECT_EQ(patterns[0].step_sequence[0], "primitive.plan");
    EXPECT_EQ(patterns[0].step_sequence[1], "primitive.execute");
    EXPECT_NEAR(patterns[0].avg_success_rate, 2.0 / 3.0, 0.01);
}

TEST_F(CompilerTest, CompileStoresPatternWithDraft) {
    evoclaw::memory::OrgLog log(test_dir_);

    append_step(log, "task-1", "planner-1", "primitive.plan", true);
    append_step(log, "task-1", "executor-1", "primitive.execute", true);
    append_step(log, "task-2", "planner-1", "primitive.plan", true);
    append_step(log, "task-2", "executor-1", "primitive.execute", true);
    append_step(log, "task-3", "planner-1", "primitive.plan", true);
    append_step(log, "task-3", "executor-1", "primitive.execute", true);

    evoclaw::compiler::Compiler compiler(log);
    const auto detected = compiler.detect_patterns(3);
    ASSERT_FALSE(detected.empty());

    const auto compiled = compiler.compile(detected.front());
    EXPECT_FALSE(compiled.id.empty());
    EXPECT_NE(compiled.description.find("umi_contract_draft"), std::string::npos);
    EXPECT_EQ(compiler.compiled_patterns().size(), 1);

    const auto status = compiler.status();
    EXPECT_EQ(status.value("compiled_count", 0), 1);
    EXPECT_GE(status.value("detected_count", 0), 1);
}

} // namespace
