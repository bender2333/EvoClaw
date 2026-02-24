#include "umi/contract.hpp"
#include "umi/validator.hpp"

#include <gtest/gtest.h>

namespace {

using evoclaw::umi::AirbagLevel;
using evoclaw::umi::ContractValidator;
using evoclaw::umi::ModuleContract;

ModuleContract build_valid_contract() {
    ModuleContract contract;
    contract.module_id = "planner.module";
    contract.version = "1.2.3";
    contract.capability.intent_tags = {"plan", "decompose"};
    contract.capability.required_tools = {"llm", "memory"};
    contract.capability.estimated_cost_token = 128.0;
    contract.capability.success_rate_threshold = 0.75;
    contract.airbag.level = AirbagLevel::BASIC;
    contract.airbag.permission_whitelist = {"read", "write"};
    contract.airbag.blast_radius_scope = {"workspace"};
    contract.airbag.reversible = true;
    contract.cost_model = {{"tokens", 128}, {"latency_ms", 450}};
    return contract;
}

TEST(UMITest, ModuleContractValidateSuccess) {
    const ModuleContract contract = build_valid_contract();
    EXPECT_TRUE(contract.validate());

    const auto result = ContractValidator::validate(contract);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errors.empty());
}

TEST(UMITest, ModuleContractValidateFailure) {
    ModuleContract contract = build_valid_contract();
    contract.module_id.clear();
    contract.capability.success_rate_threshold = 1.5;

    EXPECT_FALSE(contract.validate());

    const auto result = ContractValidator::validate(contract);
    EXPECT_FALSE(result.valid);
    EXPECT_GE(result.errors.size(), 2U);
}

TEST(UMITest, MaximumAirbagMustNotDeclarePermissions) {
    ModuleContract contract = build_valid_contract();
    contract.airbag.level = AirbagLevel::MAXIMUM;
    contract.airbag.permission_whitelist = {"read"};

    const auto result = ContractValidator::validate(contract);
    EXPECT_FALSE(result.valid);
}

TEST(UMITest, VersionMustBeSemver) {
    ModuleContract contract = build_valid_contract();
    contract.version = "v1";

    const auto result = ContractValidator::validate(contract);
    EXPECT_FALSE(result.valid);
}

} // namespace
