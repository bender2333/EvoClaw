#include "agent/executor.hpp"
#include "facade/facade.hpp"
#include "server/dashboard.hpp"
#include "server/server.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using json = nlohmann::json;

evoclaw::agent::AgentConfig make_agent_config(const std::string& id,
                                               const std::string& role,
                                               const std::vector<std::string>& intents) {
    evoclaw::agent::AgentConfig config;
    config.id = id;
    config.role = role;
    config.system_prompt = role + " agent";
    config.contract.module_id = id;
    config.contract.version = "1.0.0";
    config.contract.capability.intent_tags = intents;
    config.contract.capability.success_rate_threshold = 0.75;
    config.contract.capability.estimated_cost_token = 100.0;
    config.contract.airbag.level = evoclaw::umi::AirbagLevel::BASIC;
    config.contract.airbag.reversible = true;
    config.contract.cost_model = {{"tokens", 100.0}};
    return config;
}

json make_runtime_snapshot(const std::string& id,
                           const std::string& role,
                           const std::string& prompt,
                           const std::string& module_version) {
    return {
        {"id", id},
        {"role", role},
        {"system_prompt", prompt},
        {"temperature", 0.7},
        {"contract", {
            {"module_id", id},
            {"version", module_version},
            {"success_rate_threshold", 0.75},
            {"estimated_cost_token", 100.0},
            {"intent_tags", json::array({"execute"})},
            {"required_tools", json::array()}
        }}
    };
}

void write_json_file(const std::filesystem::path& path, const json& data) {
    std::ofstream ofs(path);
    ASSERT_TRUE(ofs.is_open());
    ofs << data.dump(2);
}

json read_json_file(const std::filesystem::path& path) {
    std::ifstream ifs(path);
    EXPECT_TRUE(ifs.is_open());
    if (!ifs.is_open()) {
        return json::array();
    }
    json parsed = json::parse(ifs, nullptr, false);
    EXPECT_FALSE(parsed.is_discarded());
    return parsed;
}

json parse_response_body(const httplib::Response& response) {
    const auto parsed = json::parse(response.body, nullptr, false);
    EXPECT_FALSE(parsed.is_discarded());
    if (parsed.is_discarded()) {
        return json::object();
    }
    return parsed;
}

httplib::Request request_with_params(
    std::initializer_list<std::pair<const std::string, std::string>> params) {
    httplib::Request req;
    for (const auto& [key, value] : params) {
        req.params.emplace(key, value);
    }
    return req;
}

class RuntimeConfigServerApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        saved_key_ = getenv("EVOCLAW_API_KEY") ? std::string(getenv("EVOCLAW_API_KEY")) : "";
        saved_home_ = getenv("HOME") ? std::string(getenv("HOME")) : "";
        unsetenv("EVOCLAW_API_KEY");

        test_dir_ = std::filesystem::temp_directory_path() / ("evoclaw_server_api_" + evoclaw::generate_uuid());
        std::filesystem::create_directories(test_dir_);
        setenv("HOME", test_dir_.string().c_str(), 1);

        seed_runtime_config_history();

        evoclaw::facade::EvoClawFacade::Config config;
        config.log_dir = test_dir_;
        facade_ = std::make_unique<evoclaw::facade::EvoClawFacade>(config);
        facade_->initialize();
        facade_->register_agent(std::make_shared<evoclaw::agent::Executor>(
            make_agent_config(agent_id_, "executor", {"execute"})));

        server_ = std::make_unique<evoclaw::server::EvoClawServer>(*facade_);
    }

    void TearDown() override {
        server_.reset();
        facade_.reset();

        if (!saved_key_.empty()) {
            setenv("EVOCLAW_API_KEY", saved_key_.c_str(), 1);
        }
        if (!saved_home_.empty()) {
            setenv("HOME", saved_home_.c_str(), 1);
        }

        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    void seed_runtime_config_history() {
        const auto s0 = make_runtime_snapshot(agent_id_, "executor", "runtime api prompt v0", "1.0.0");
        const auto s1 = make_runtime_snapshot(agent_id_, "executor", "runtime api prompt v1", "1.0.1");
        const auto s2 = make_runtime_snapshot(agent_id_, "executor", "runtime api prompt v2", "1.0.2");

        json history = json::array({
            {
                {"agent_id", agent_id_},
                {"version", 1U},
                {"proposal_id", "proposal-api-1"},
                {"before", s0},
                {"after", s1},
                {"diff", {{"system_prompt", {{"before", s0["system_prompt"]}, {"after", s1["system_prompt"]}}}}},
                {"changed_at", "2026-03-01T12:00:00"}
            },
            {
                {"agent_id", agent_id_},
                {"version", 2U},
                {"proposal_id", "proposal-api-2"},
                {"before", s1},
                {"after", s2},
                {"diff", {{"system_prompt", {{"before", s1["system_prompt"]}, {"after", s2["system_prompt"]}}}}},
                {"changed_at", "2026-03-01T12:01:00"}
            }
        });

        write_json_file(test_dir_ / "runtime_config_versions.json", {
            {agent_id_, 2U}
        });
        write_json_file(test_dir_ / "runtime_config_history.json", history);
    }

    std::filesystem::path test_dir_;
    std::string saved_key_;
    std::string saved_home_;
    const std::string agent_id_ = "executor-api";
    std::unique_ptr<evoclaw::facade::EvoClawFacade> facade_;
    std::unique_ptr<evoclaw::server::EvoClawServer> server_;
};

TEST_F(RuntimeConfigServerApiTest, ExposesRuntimeConfigVersionHistoryAndDiffEndpoints) {
    {
        auto req = request_with_params({{"agent_id", agent_id_}});
        httplib::Response res;
        server_->handle_runtime_config_version(req, res);

        EXPECT_EQ(res.status, 200);
        const auto payload = parse_response_body(res);
        EXPECT_EQ(payload["agent_id"].get<std::string>(), agent_id_);
        EXPECT_EQ(payload["version"].get<std::uint64_t>(), 2U);
    }

    {
        auto req = request_with_params({{"agent_id", agent_id_}});
        httplib::Response res;
        server_->handle_runtime_config_history(req, res);

        EXPECT_EQ(res.status, 200);
        const auto payload = parse_response_body(res);
        ASSERT_TRUE(payload.is_array());
        ASSERT_EQ(payload.size(), 2U);
        EXPECT_EQ(payload[0]["version"].get<std::uint64_t>(), 1U);
        EXPECT_EQ(payload[1]["version"].get<std::uint64_t>(), 2U);
    }

    {
        auto req = request_with_params({
            {"agent_id", agent_id_},
            {"from_version", "0"},
            {"to_version", "2"}
        });
        httplib::Response res;
        server_->handle_runtime_config_diff(req, res);

        EXPECT_EQ(res.status, 200);
        const auto payload = parse_response_body(res);
        EXPECT_EQ(payload["agent_id"].get<std::string>(), agent_id_);
        EXPECT_EQ(payload["from_version"].get<std::uint64_t>(), 0U);
        EXPECT_EQ(payload["to_version"].get<std::uint64_t>(), 2U);
        EXPECT_TRUE(payload["diff"].contains("system_prompt"));
    }
}

TEST_F(RuntimeConfigServerApiTest, RuntimeConfigQueryValidationAndErrorStatusMappingAreCorrect) {
    {
        httplib::Request req;
        httplib::Response res;
        server_->handle_runtime_config_version(req, res);

        EXPECT_EQ(res.status, 400);
        const auto payload = parse_response_body(res);
        EXPECT_FALSE(payload.value("ok", true));
        EXPECT_TRUE(payload["error"].is_string());
    }

    {
        auto req = request_with_params({{"agent_id", "does-not-exist"}});
        httplib::Response res;
        server_->handle_runtime_config_version(req, res);

        EXPECT_EQ(res.status, 404);
        const auto payload = parse_response_body(res);
        EXPECT_EQ(payload.value("error", std::string()), "agent_not_found");
    }

    {
        auto req = request_with_params({{"agent_id", "does-not-exist"}});
        httplib::Response res;
        server_->handle_runtime_config_history(req, res);

        EXPECT_EQ(res.status, 404);
        const auto payload = parse_response_body(res);
        EXPECT_EQ(payload.value("error", std::string()), "agent_not_found");
    }

    {
        auto req = request_with_params({
            {"agent_id", agent_id_},
            {"to_version", "1"}
        });
        httplib::Response res;
        server_->handle_runtime_config_diff(req, res);

        EXPECT_EQ(res.status, 400);
        const auto payload = parse_response_body(res);
        EXPECT_FALSE(payload.value("ok", true));
        EXPECT_TRUE(payload["error"].is_string());
    }

    {
        auto req = request_with_params({
            {"agent_id", agent_id_},
            {"from_version", "abc"},
            {"to_version", "1"}
        });
        httplib::Response res;
        server_->handle_runtime_config_diff(req, res);

        EXPECT_EQ(res.status, 400);
        const auto payload = parse_response_body(res);
        EXPECT_FALSE(payload.value("ok", true));
        EXPECT_TRUE(payload["error"].is_string());
    }

    {
        auto req = request_with_params({
            {"agent_id", agent_id_},
            {"from_version", "2"},
            {"to_version", "1"}
        });
        httplib::Response res;
        server_->handle_runtime_config_diff(req, res);

        EXPECT_EQ(res.status, 400);
        const auto payload = parse_response_body(res);
        EXPECT_EQ(payload.value("error", std::string()), "invalid_version_range");
    }

    {
        auto req = request_with_params({
            {"agent_id", agent_id_},
            {"from_version", "0"},
            {"to_version", "99"}
        });
        httplib::Response res;
        server_->handle_runtime_config_diff(req, res);

        EXPECT_EQ(res.status, 404);
        const auto payload = parse_response_body(res);
        EXPECT_EQ(payload.value("error", std::string()), "version_not_found");
    }
}

TEST_F(RuntimeConfigServerApiTest, PruneEndpointReturnsStatusAndPersistsHistory) {
    httplib::Request prune_req;
    prune_req.body = R"({"keep_last_per_agent":1})";
    httplib::Response prune_res;

    server_->handle_runtime_config_history_prune(prune_req, prune_res);

    EXPECT_EQ(prune_res.status, 200);
    const auto prune_payload = parse_response_body(prune_res);
    EXPECT_TRUE(prune_payload.value("ok", false));
    EXPECT_EQ(prune_payload["keep_last_per_agent"].get<std::size_t>(), 1U);
    ASSERT_TRUE(prune_payload.contains("runtime_config"));
    EXPECT_EQ(prune_payload["runtime_config"]["history_entries"].get<std::size_t>(), 1U);

    {
        auto history_req = request_with_params({{"agent_id", agent_id_}});
        httplib::Response history_res;
        server_->handle_runtime_config_history(history_req, history_res);

        EXPECT_EQ(history_res.status, 200);
        const auto history_payload = parse_response_body(history_res);
        ASSERT_EQ(history_payload.size(), 1U);
        EXPECT_EQ(history_payload[0]["version"].get<std::uint64_t>(), 2U);
    }

    const auto persisted_history = read_json_file(test_dir_ / "runtime_config_history.json");
    ASSERT_TRUE(persisted_history.is_array());
    ASSERT_EQ(persisted_history.size(), 1U);
    EXPECT_EQ(persisted_history[0]["version"].get<std::uint64_t>(), 2U);
}

TEST_F(RuntimeConfigServerApiTest, PruneValidationErrorsReturnBuildErrorWith400) {
    {
        httplib::Request req;
        req.body = "{";
        httplib::Response res;
        server_->handle_runtime_config_history_prune(req, res);

        EXPECT_EQ(res.status, 400);
        const auto payload = parse_response_body(res);
        EXPECT_FALSE(payload.value("ok", true));
        EXPECT_TRUE(payload["error"].is_string());
    }

    {
        httplib::Request req;
        req.body = "{}";
        httplib::Response res;
        server_->handle_runtime_config_history_prune(req, res);

        EXPECT_EQ(res.status, 400);
        const auto payload = parse_response_body(res);
        EXPECT_FALSE(payload.value("ok", true));
        EXPECT_TRUE(payload["error"].is_string());
    }

    {
        httplib::Request req;
        req.body = R"({"keep_last_per_agent":-1})";
        httplib::Response res;
        server_->handle_runtime_config_history_prune(req, res);

        EXPECT_EQ(res.status, 400);
        const auto payload = parse_response_body(res);
        EXPECT_FALSE(payload.value("ok", true));
        EXPECT_TRUE(payload["error"].is_string());
    }
}

TEST(DashboardHtmlTest, IncludesRuntimeConfigSummaryPruneAndInspectorControls) {
    const std::string html = evoclaw::server::dashboard::kDashboardHtml;
    EXPECT_NE(html.find("Runtime Tracked Agents"), std::string::npos);
    EXPECT_NE(html.find("Runtime History Entries"), std::string::npos);
    EXPECT_NE(html.find("runtime_version"), std::string::npos);
    EXPECT_NE(html.find("runtime_history_count"), std::string::npos);
    EXPECT_NE(html.find("runtime_latest_changed_at"), std::string::npos);
    EXPECT_NE(html.find("Runtime Config Inspector"), std::string::npos);
    EXPECT_NE(html.find("Inspect Runtime"), std::string::npos);
    EXPECT_NE(html.find("from_version"), std::string::npos);
    EXPECT_NE(html.find("to_version"), std::string::npos);
    EXPECT_NE(html.find("runtime-diff-output"), std::string::npos);
    EXPECT_NE(html.find("/api/runtime-config/history?"), std::string::npos);
    EXPECT_NE(html.find("/api/runtime-config/diff?"), std::string::npos);
    EXPECT_NE(html.find("keep_last_per_agent"), std::string::npos);
    EXPECT_NE(html.find("/api/runtime-config/history/prune"), std::string::npos);
}

} // namespace
