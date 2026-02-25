#pragma once

#include "core/types.hpp"
#include "llm/llm_client.hpp"

#include <memory>
#include <string>

namespace evoclaw::slp {

enum class Granularity {
    SMALL,
    MEDIUM,
    LARGE
};

struct SemanticPrimitive {
    std::string id;
    Granularity granularity = Granularity::SMALL;
    std::string content;
    std::string source_id;
    int token_count = 0;
    Timestamp created_at = now();
};

class SLPCompressor {
public:
    explicit SLPCompressor(std::shared_ptr<llm::LLMClient> llm_client);

    [[nodiscard]] SemanticPrimitive compress(const std::string& text, Granularity granularity) const;

    struct MultiGranularitySummary {
        SemanticPrimitive small;
        SemanticPrimitive medium;
        SemanticPrimitive large;
    };

    [[nodiscard]] MultiGranularitySummary compress_all(const std::string& text) const;

    [[nodiscard]] static SemanticPrimitive mock_compress(const std::string& text, Granularity granularity);

private:
    std::shared_ptr<llm::LLMClient> llm_client_;

    [[nodiscard]] static int max_tokens_for(Granularity granularity);
    [[nodiscard]] static std::string prompt_for(Granularity granularity);
};

} // namespace evoclaw::slp
