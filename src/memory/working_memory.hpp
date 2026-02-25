#pragma once

#include "slp/semantic_primitive.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace evoclaw::memory {

class WorkingMemory {
public:
    void store(const std::string& key, nlohmann::json value);
    [[nodiscard]] std::optional<nlohmann::json> retrieve(const std::string& key) const;
    void clear();
    void set_compressor(std::shared_ptr<slp::SLPCompressor> compressor);

    [[nodiscard]] std::string one_line_summary() const;
    [[nodiscard]] std::string paragraph_summary() const;

private:
    std::unordered_map<std::string, nlohmann::json> data_;
    std::vector<std::string> conversation_history_;
    std::shared_ptr<slp::SLPCompressor> compressor_;
};

} // namespace evoclaw::memory
