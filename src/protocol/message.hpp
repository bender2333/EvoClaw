#pragma once

#include "core/types.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace evoclaw::protocol {

enum class Performative {
    INFORM,
    REQUEST,
    PROPOSE,
    CRITIQUE,
    VOTE,
    REPORT,
    TENSION
};

NLOHMANN_JSON_SERIALIZE_ENUM(Performative, {
    {Performative::INFORM, "inform"},
    {Performative::REQUEST, "request"},
    {Performative::PROPOSE, "propose"},
    {Performative::CRITIQUE, "critique"},
    {Performative::VOTE, "vote"},
    {Performative::REPORT, "report"},
    {Performative::TENSION, "tension"}
})

struct Message {
    MessageId id;
    Performative performative{};
    AgentId sender;
    std::vector<AgentId> receivers;
    nlohmann::json payload;
    nlohmann::json metadata;
    std::optional<MessageId> parent_id;

    Message();

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static std::optional<Message> from_json(const std::string& json_str);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Message, id, performative, sender, receivers, payload, metadata, parent_id)

inline Message::Message()
    : id(generate_uuid()), payload(nlohmann::json::object()), metadata(nlohmann::json::object()) {}

inline std::string Message::to_json() const {
    return nlohmann::json(*this).dump(2);
}

inline std::optional<Message> Message::from_json(const std::string& json_str) {
    try {
        return nlohmann::json::parse(json_str).get<Message>();
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace evoclaw::protocol
