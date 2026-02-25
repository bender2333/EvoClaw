#include "compiler/compiler.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace evoclaw::compiler {
namespace {

struct PatternAccumulator {
    std::vector<std::string> sequence;
    int observations = 0;
    int success_count = 0;
    Timestamp first_seen{};
    Timestamp last_seen{};
};

std::string make_pattern_key(const std::vector<std::string>& sequence) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        if (i > 0U) {
            oss << "->";
        }
        oss << sequence[i];
    }
    return oss.str();
}

std::string make_pattern_id(const std::string& key) {
    const auto hashed = std::hash<std::string>{}(key);
    std::ostringstream oss;
    oss << "pat-" << std::hex << static_cast<std::uint64_t>(hashed);
    return oss.str();
}

std::string extract_step(const memory::OrgLogEntry& entry) {
    const auto& metadata = entry.metadata;

    if (metadata.contains("primitive_id") && metadata["primitive_id"].is_string()) {
        return metadata["primitive_id"].get<std::string>();
    }
    if (metadata.contains("slp_primitive_id") && metadata["slp_primitive_id"].is_string()) {
        return metadata["slp_primitive_id"].get<std::string>();
    }
    if (metadata.contains("primitive") && metadata["primitive"].is_string()) {
        return metadata["primitive"].get<std::string>();
    }

    return entry.agent_id;
}

bool extract_success(const memory::OrgLogEntry& entry) {
    const auto& metadata = entry.metadata;
    if (metadata.contains("success") && metadata["success"].is_boolean()) {
        return metadata["success"].get<bool>();
    }
    return entry.user_feedback_positive;
}

nlohmann::json pattern_to_json(const Pattern& pattern) {
    return {
        {"id", pattern.id},
        {"name", pattern.name},
        {"description", pattern.description},
        {"step_sequence", pattern.step_sequence},
        {"observation_count", pattern.observation_count},
        {"avg_success_rate", pattern.avg_success_rate},
        {"created_at", timestamp_to_string(pattern.created_at)},
        {"last_seen", timestamp_to_string(pattern.last_seen)}
    };
}

std::string build_umi_draft(const Pattern& pattern) {
    const nlohmann::json draft = {
        {"module_id", "compiled." + pattern.id},
        {"version", "0.1.0"},
        {"capability", {
            {"intent_tags", pattern.step_sequence.empty()
                                ? nlohmann::json::array()
                                : nlohmann::json::array({pattern.step_sequence.front()})},
            {"required_tools", nlohmann::json::array()},
            {"estimated_cost_token", 128.0},
            {"success_rate_threshold", std::clamp(pattern.avg_success_rate, 0.0, 1.0)}
        }}
    };
    return draft.dump();
}

} // namespace

Compiler::Compiler(const memory::OrgLog& org_log)
    : org_log_(org_log) {}

std::vector<Pattern> Compiler::detect_patterns(const int min_occurrences) const {
    if (min_occurrences <= 0) {
        return {};
    }

    const std::vector<memory::OrgLogEntry> entries = org_log_.all_entries();
    if (entries.empty()) {
        return {};
    }

    std::unordered_map<TaskId, std::vector<memory::OrgLogEntry>> by_task;
    by_task.reserve(entries.size());
    for (const auto& entry : entries) {
        if (entry.task_id.empty()) {
            continue;
        }
        by_task[entry.task_id].push_back(entry);
    }

    std::unordered_map<std::string, PatternAccumulator> patterns;
    patterns.reserve(by_task.size());

    for (auto& [task_id, task_entries] : by_task) {
        (void)task_id;

        if (task_entries.empty()) {
            continue;
        }

        std::sort(task_entries.begin(), task_entries.end(), [](const memory::OrgLogEntry& lhs,
                                                               const memory::OrgLogEntry& rhs) {
            return lhs.timestamp < rhs.timestamp;
        });

        std::vector<std::string> sequence;
        sequence.reserve(task_entries.size());
        for (const auto& entry : task_entries) {
            std::string step = extract_step(entry);
            if (!step.empty()) {
                sequence.push_back(std::move(step));
            }
        }
        if (sequence.empty()) {
            continue;
        }

        const std::string key = make_pattern_key(sequence);
        PatternAccumulator& acc = patterns[key];
        if (acc.observations == 0) {
            acc.sequence = sequence;
            acc.first_seen = task_entries.front().timestamp;
            acc.last_seen = task_entries.back().timestamp;
        } else {
            if (task_entries.front().timestamp < acc.first_seen) {
                acc.first_seen = task_entries.front().timestamp;
            }
            if (task_entries.back().timestamp > acc.last_seen) {
                acc.last_seen = task_entries.back().timestamp;
            }
        }

        ++acc.observations;
        if (extract_success(task_entries.back())) {
            ++acc.success_count;
        }
    }

    std::vector<Pattern> detected;
    for (const auto& [key, acc] : patterns) {
        if (acc.observations < min_occurrences) {
            continue;
        }

        Pattern pattern;
        pattern.id = make_pattern_id(key);
        pattern.name = "pattern_" + pattern.id.substr(std::min<std::size_t>(4, pattern.id.size()));
        pattern.description = "Observed repeated sequence with " +
                              std::to_string(acc.sequence.size()) + " steps.";
        pattern.step_sequence = acc.sequence;
        pattern.observation_count = acc.observations;
        pattern.avg_success_rate =
            static_cast<double>(acc.success_count) / static_cast<double>(std::max(acc.observations, 1));
        pattern.created_at = acc.first_seen;
        pattern.last_seen = acc.last_seen;
        detected.push_back(std::move(pattern));
    }

    std::sort(detected.begin(), detected.end(), [](const Pattern& lhs, const Pattern& rhs) {
        if (lhs.observation_count != rhs.observation_count) {
            return lhs.observation_count > rhs.observation_count;
        }
        return lhs.id < rhs.id;
    });

    return detected;
}

Pattern Compiler::compile(const Pattern& pattern) const {
    Pattern compiled = pattern;
    const std::string key = make_pattern_key(compiled.step_sequence);

    if (compiled.id.empty()) {
        compiled.id = make_pattern_id(key);
    }
    if (compiled.name.empty()) {
        compiled.name = "compiled_" + compiled.id;
    }
    if (compiled.created_at.time_since_epoch() == Timestamp::duration::zero()) {
        compiled.created_at = now();
    }
    compiled.last_seen = now();

    const std::string umi_draft = build_umi_draft(compiled);
    compiled.description = compiled.description + " [umi_contract_draft=" + umi_draft + "]";

    auto existing = std::find_if(compiled_patterns_.begin(), compiled_patterns_.end(),
                                 [&compiled](const Pattern& item) {
                                     return item.id == compiled.id;
                                 });
    if (existing == compiled_patterns_.end()) {
        compiled_patterns_.push_back(compiled);
    } else {
        *existing = compiled;
    }

    return compiled;
}

const std::vector<Pattern>& Compiler::compiled_patterns() const {
    return compiled_patterns_;
}

nlohmann::json Compiler::status() const {
    const auto detected = detect_patterns();

    nlohmann::json payload;
    payload["detected_count"] = detected.size();
    payload["compiled_count"] = compiled_patterns_.size();
    payload["detected"] = nlohmann::json::array();
    payload["compiled"] = nlohmann::json::array();

    for (const auto& pattern : detected) {
        payload["detected"].push_back(pattern_to_json(pattern));
    }
    for (const auto& pattern : compiled_patterns_) {
        payload["compiled"].push_back(pattern_to_json(pattern));
    }

    return payload;
}

} // namespace evoclaw::compiler
