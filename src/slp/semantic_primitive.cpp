#include "slp/semantic_primitive.hpp"

#include <algorithm>
#include <functional>
#include <sstream>
#include <utility>

namespace evoclaw::slp {
namespace {

int estimate_tokens(const std::string& text) {
    std::istringstream iss(text);
    int words = 0;
    std::string token;
    while (iss >> token) {
        ++words;
    }
    return words;
}

std::string truncate_to_token_budget(const std::string& text, const int max_tokens) {
    if (max_tokens <= 0 || text.empty()) {
        return {};
    }

    std::istringstream iss(text);
    std::ostringstream oss;
    std::string token;
    int written = 0;
    while (iss >> token) {
        if (written > 0) {
            oss << ' ';
        }
        oss << token;
        ++written;
        if (written >= max_tokens) {
            break;
        }
    }
    return oss.str();
}

std::string source_id_for(const std::string& text) {
    const std::size_t h = std::hash<std::string>{}(text);
    return "src-" + std::to_string(h);
}

} // namespace

SLPCompressor::SLPCompressor(std::shared_ptr<llm::LLMClient> llm_client)
    : llm_client_(std::move(llm_client)) {}

SemanticPrimitive SLPCompressor::compress(const std::string& text, const Granularity granularity) const {
    if (text.empty()) {
        SemanticPrimitive empty;
        empty.id = generate_uuid();
        empty.granularity = granularity;
        empty.source_id = source_id_for(text);
        empty.content = "";
        empty.token_count = 0;
        empty.created_at = now();
        return empty;
    }

    if (llm_client_ && !llm_client_->is_mock_mode()) {
        const llm::LLMResponse response = llm_client_->ask(prompt_for(granularity), text);
        if (response.success && !response.content.empty()) {
            SemanticPrimitive primitive;
            primitive.id = generate_uuid();
            primitive.granularity = granularity;
            primitive.source_id = source_id_for(text);
            primitive.content = response.content;
            primitive.token_count =
                response.completion_tokens > 0 ? response.completion_tokens : estimate_tokens(response.content);
            primitive.created_at = now();
            return primitive;
        }
    }

    return mock_compress(text, granularity);
}

SLPCompressor::MultiGranularitySummary SLPCompressor::compress_all(const std::string& text) const {
    return {
        .small = compress(text, Granularity::SMALL),
        .medium = compress(text, Granularity::MEDIUM),
        .large = compress(text, Granularity::LARGE)
    };
}

SemanticPrimitive SLPCompressor::mock_compress(const std::string& text, const Granularity granularity) {
    const int max_tokens = max_tokens_for(granularity);
    const std::string truncated = truncate_to_token_budget(text, max_tokens);

    SemanticPrimitive primitive;
    primitive.id = generate_uuid();
    primitive.granularity = granularity;
    primitive.content = truncated;
    primitive.source_id = source_id_for(text);
    primitive.token_count = std::min(estimate_tokens(truncated), max_tokens);
    primitive.created_at = now();
    return primitive;
}

int SLPCompressor::max_tokens_for(const Granularity granularity) {
    switch (granularity) {
        case Granularity::SMALL:
            return 50;
        case Granularity::MEDIUM:
            return 200;
        case Granularity::LARGE:
            return 500;
    }
    return 50;
}

std::string SLPCompressor::prompt_for(const Granularity granularity) {
    switch (granularity) {
        case Granularity::SMALL:
            return "Summarize the text in one concise sentence under 50 tokens.";
        case Granularity::MEDIUM:
            return "Summarize the text in one concise paragraph under 200 tokens.";
        case Granularity::LARGE:
            return "Summarize the text thoroughly under 500 tokens while preserving key details.";
    }
    return "Summarize the text.";
}

} // namespace evoclaw::slp
