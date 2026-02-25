#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace evoclaw::governance {

using json = nlohmann::json;

struct Constitution {
    std::string mission;
    std::vector<std::string> values;
    std::vector<std::string> guardrails;
    json evolution_policy = json::object();

    [[nodiscard]] bool check_violation(const json& action) const {
        if (guardrails.empty()) {
            return false;
        }

        std::string description;
        if (action.is_string()) {
            description = action.get<std::string>();
        } else if (action.is_object()) {
            if (action.contains("description") && action["description"].is_string()) {
                description = action["description"].get<std::string>();
            } else if (action.contains("action") && action["action"].is_string()) {
                description = action["action"].get<std::string>();
            }
        }

        if (description.empty()) {
            return false;
        }

        std::transform(description.begin(), description.end(), description.begin(),
                       [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

        for (std::string keyword : guardrails) {
            if (keyword.empty()) {
                continue;
            }
            std::transform(keyword.begin(), keyword.end(), keyword.begin(),
                           [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (description.find(keyword) != std::string::npos) {
                return true;
            }
        }

        return false;
    }
};

} // namespace evoclaw::governance
