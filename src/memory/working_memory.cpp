#include "memory/working_memory.hpp"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

namespace evoclaw::memory {
namespace {

std::vector<std::string> sorted_keys(const std::unordered_map<std::string, nlohmann::json>& data) {
    std::vector<std::string> keys;
    keys.reserve(data.size());
    for (const auto& [key, _] : data) {
        (void)_;
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

std::string shorten(std::string value, const std::size_t max_len) {
    if (value.size() <= max_len) {
        return value;
    }
    value.resize(max_len);
    value += "...";
    return value;
}

} // namespace

void WorkingMemory::store(const std::string& key, nlohmann::json value) {
    data_[key] = std::move(value);
    conversation_history_.push_back(key);
}

std::optional<nlohmann::json> WorkingMemory::retrieve(const std::string& key) const {
    const auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void WorkingMemory::clear() {
    data_.clear();
    conversation_history_.clear();
}

void WorkingMemory::set_compressor(std::shared_ptr<slp::SLPCompressor> compressor) {
    compressor_ = std::move(compressor);
}

std::string WorkingMemory::one_line_summary() const {
    if (data_.empty()) {
        return "WorkingMemory: empty";
    }

    const auto keys = sorted_keys(data_);
    constexpr std::size_t kDisplayLimit = 3;

    std::ostringstream oss;
    oss << "WorkingMemory: " << data_.size() << " keys [";
    for (std::size_t i = 0; i < std::min(keys.size(), kDisplayLimit); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << keys[i];
    }
    if (keys.size() > kDisplayLimit) {
        oss << ", ...";
    }
    oss << "]";

    const std::string summary = oss.str();
    if (!compressor_) {
        return summary;
    }

    const slp::SemanticPrimitive compressed = compressor_->compress(summary, slp::Granularity::SMALL);
    return compressed.content.empty() ? summary : compressed.content;
}

std::string WorkingMemory::paragraph_summary() const {
    if (data_.empty()) {
        return "WorkingMemory currently has no stored context.";
    }

    const auto keys = sorted_keys(data_);
    std::ostringstream oss;
    oss << "Working memory holds " << data_.size() << " key-value entries. ";
    oss << "Recently touched keys: ";

    constexpr std::size_t kHistoryLimit = 5;
    const std::size_t history_start = conversation_history_.size() > kHistoryLimit
                                          ? conversation_history_.size() - kHistoryLimit
                                          : 0;
    if (conversation_history_.empty()) {
        oss << "none.";
    } else {
        for (std::size_t i = history_start; i < conversation_history_.size(); ++i) {
            if (i > history_start) {
                oss << ", ";
            }
            oss << conversation_history_[i];
        }
        oss << ".";
    }

    oss << " Current snapshot: ";
    constexpr std::size_t kValueLimit = 5;
    for (std::size_t i = 0; i < std::min(keys.size(), kValueLimit); ++i) {
        if (i > 0) {
            oss << " | ";
        }
        const auto& key = keys[i];
        oss << key << "=" << shorten(data_.at(key).dump(), 80);
    }
    if (keys.size() > kValueLimit) {
        oss << " | ...";
    }

    const std::string summary = oss.str();
    if (!compressor_) {
        return summary;
    }

    const slp::SemanticPrimitive compressed = compressor_->compress(summary, slp::Granularity::MEDIUM);
    return compressed.content.empty() ? summary : compressed.content;
}

} // namespace evoclaw::memory
