#include "slp/semantic_primitive.hpp"

#include <gtest/gtest.h>

namespace {

TEST(SLPTest, MockCompressSmall) {
    auto result = evoclaw::slp::SLPCompressor::mock_compress(
        "This is a long text that should be compressed into a small summary with limited tokens",
        evoclaw::slp::Granularity::SMALL
    );

    EXPECT_EQ(result.granularity, evoclaw::slp::Granularity::SMALL);
    EXPECT_FALSE(result.content.empty());
    EXPECT_LE(result.token_count, 50);
    EXPECT_FALSE(result.id.empty());
    EXPECT_FALSE(result.source_id.empty());
}

TEST(SLPTest, MockCompressMedium) {
    auto result = evoclaw::slp::SLPCompressor::mock_compress(
        "This is a long text that should be compressed into a medium summary with more tokens allowed for detail",
        evoclaw::slp::Granularity::MEDIUM
    );

    EXPECT_EQ(result.granularity, evoclaw::slp::Granularity::MEDIUM);
    EXPECT_FALSE(result.content.empty());
    EXPECT_LE(result.token_count, 200);
}

TEST(SLPTest, MockCompressLarge) {
    auto result = evoclaw::slp::SLPCompressor::mock_compress(
        "This is a long text that should be compressed into a large summary preserving key details",
        evoclaw::slp::Granularity::LARGE
    );

    EXPECT_EQ(result.granularity, evoclaw::slp::Granularity::LARGE);
    EXPECT_FALSE(result.content.empty());
    EXPECT_LE(result.token_count, 500);
}

TEST(SLPTest, CompressAllThreeGranularities) {
    auto compressor = evoclaw::slp::SLPCompressor(nullptr);  // mock mode
    auto summary = compressor.compress_all(
        "Microservices architecture is a design approach that structures an application as a collection of small independent services"
    );

    EXPECT_EQ(summary.small.granularity, evoclaw::slp::Granularity::SMALL);
    EXPECT_EQ(summary.medium.granularity, evoclaw::slp::Granularity::MEDIUM);
    EXPECT_EQ(summary.large.granularity, evoclaw::slp::Granularity::LARGE);
    EXPECT_LE(summary.small.token_count, 50);
    EXPECT_LE(summary.medium.token_count, 200);
    EXPECT_LE(summary.large.token_count, 500);
}

TEST(SLPTest, EmptyTextReturnsEmpty) {
    auto result = evoclaw::slp::SLPCompressor::mock_compress("", evoclaw::slp::Granularity::SMALL);
    EXPECT_TRUE(result.content.empty());
    EXPECT_EQ(result.token_count, 0);
}

} // namespace
