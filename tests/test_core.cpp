#include "core/types.hpp"

#include <gtest/gtest.h>

#include <regex>
#include <string>
#include <unordered_set>

namespace {

TEST(CoreTypesTest, GenerateUuidFormatIsV4) {
    const std::string uuid = evoclaw::generate_uuid();

    static const std::regex pattern(
        R"(^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$)");
    EXPECT_TRUE(std::regex_match(uuid, pattern));
}

TEST(CoreTypesTest, GenerateUuidIsLikelyUnique) {
    std::unordered_set<std::string> ids;
    ids.reserve(256);

    for (int i = 0; i < 256; ++i) {
        ids.insert(evoclaw::generate_uuid());
    }

    EXPECT_EQ(ids.size(), 256U);
}

TEST(CoreTypesTest, TimestampToStringHasIsoLikeFormat) {
    const std::string ts = evoclaw::timestamp_to_string(evoclaw::now());

    static const std::regex pattern(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$)");
    EXPECT_TRUE(std::regex_match(ts, pattern));
}

} // namespace
