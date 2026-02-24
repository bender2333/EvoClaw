#include "protocol/message.hpp"

#include <iostream>

int main() {
    evoclaw::protocol::Message msg;
    msg.performative = evoclaw::protocol::Performative::INFORM;
    msg.sender = "example_agent";
    msg.receivers = {"observer"};
    msg.payload = {{"status", "ok"}, {"step", "phase1"}};

    const std::string encoded = msg.to_json();
    std::cout << "Encoded message:\n" << encoded << "\n";

    const auto decoded = evoclaw::protocol::Message::from_json(encoded);
    if (!decoded.has_value()) {
        std::cerr << "Failed to decode message\n";
        return 1;
    }

    std::cout << "Decoded id: " << decoded->id << "\n";
    std::cout << "Decoded sender: " << decoded->sender << "\n";
    return 0;
}
