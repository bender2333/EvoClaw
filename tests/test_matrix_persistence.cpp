#include "agent/executor.hpp"
#include "agent/planner.hpp"
#include "router/router.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>

namespace {

evoclaw::umi::ModuleContract make_contract(std::string module_id,
                                           const std::vector<std::string>& intents,
                                           const double success_rate_threshold) {
    evoclaw::umi::ModuleContract contract;
    contract.module_id = std::move(module_id);
    contract.version = "1.0.0";
    contract.capability.intent_tags = intents;
    contract.capability.required_tools = {};
    contract.capability.success_rate_threshold = success_rate_threshold;
    contract.capability.estimated_cost_token = 100.0;
    contract.airbag.level = evoclaw::umi::AirbagLevel::BASIC;
    contract.airbag.reversible = true;
    contract.cost_model = {{"tokens", 100.0}};
    return contract;
}

evoclaw::agent::AgentConfig make_config(const std::string& id,
                                        const std::string& role,
                                        evoclaw::umi::ModuleContract contract) {
    evoclaw::agent::AgentConfig config;
    config.id = id;
    config.role = role;
    config.system_prompt = "test";
    config.contract = std::move(contract);
    config.temperature = 0.3;
    return config;
}

nlohmann::json read_json_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    EXPECT_TRUE(in.is_open()) << "Unable to read " << path;

    nlohmann::json parsed;
    if (in.is_open()) {
        in >> parsed;
    }
    return parsed;
}

class MatrixPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_matrix_" + evoclaw::generate_uuid());
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::filesystem::path test_dir_;
};

TEST_F(MatrixPersistenceTest, SaveAndLoad) {
    evoclaw::router::RoutingConfig config;
    config.epsilon = 0.0;
    evoclaw::router::Router router(config);

    auto planner = std::make_shared<evoclaw::agent::Planner>(
        make_config("planner-1", "planner", make_contract("planner.mod", {"plan"}, 0.9)));
    auto executor = std::make_shared<evoclaw::agent::Executor>(
        make_config("executor-1", "executor", make_contract("executor.mod", {"plan"}, 0.5)));

    router.register_agent(planner);
    router.register_agent(executor);

    evoclaw::agent::Task task;
    task.id = "task-1";
    task.description = "plan";
    task.type = evoclaw::TaskType::EXECUTE;
    task.context = {{"intent", "plan"}};

    for (int i = 0; i < 5; ++i) {
        task.id = "task-" + std::to_string(i);
        const auto routed = router.route(task);
        ASSERT_TRUE(routed.has_value());
    }

    const auto file_a = test_dir_ / "matrix_a.json";
    const auto file_b = test_dir_ / "matrix_b.json";

    router.save_matrix(file_a);
    ASSERT_TRUE(std::filesystem::exists(file_a));

    evoclaw::router::Router reloaded(config);
    reloaded.load_matrix(file_a);
    reloaded.save_matrix(file_b);

    const auto json_a = read_json_file(file_a);
    const auto json_b = read_json_file(file_b);

    ASSERT_TRUE(json_a.contains("matrix"));
    ASSERT_TRUE(json_b.contains("matrix"));
    EXPECT_EQ(json_a["matrix"], json_b["matrix"]);
}

TEST_F(MatrixPersistenceTest, LoadNonexistentFileIsNoOp) {
    evoclaw::router::Router router;
    const auto missing = test_dir_ / "does_not_exist.json";

    EXPECT_FALSE(std::filesystem::exists(missing));
    EXPECT_NO_THROW(router.load_matrix(missing));

    const auto saved = test_dir_ / "saved.json";
    router.save_matrix(saved);
    const auto json = read_json_file(saved);
    EXPECT_EQ(json.value("version", std::string()), "1.0");
    EXPECT_TRUE(json.contains("matrix"));
    EXPECT_TRUE(json["matrix"].is_object());
}

} // namespace
