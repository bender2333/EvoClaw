#pragma once

#include "compiler/pattern.hpp"
#include "memory/org_log.hpp"

#include <nlohmann/json.hpp>

#include <vector>

namespace evoclaw::compiler {

class Compiler {
public:
    explicit Compiler(const memory::OrgLog& org_log);

    [[nodiscard]] std::vector<Pattern> detect_patterns(int min_occurrences = 3) const;
    [[nodiscard]] Pattern compile(const Pattern& pattern) const;
    [[nodiscard]] const std::vector<Pattern>& compiled_patterns() const;
    [[nodiscard]] nlohmann::json status() const;

private:
    const memory::OrgLog& org_log_;
    mutable std::vector<Pattern> compiled_patterns_;
};

} // namespace evoclaw::compiler
