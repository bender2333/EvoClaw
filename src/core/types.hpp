#pragma once

#include <chrono>
#include <ctime>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace evoclaw {

using MessageId = std::string;
using AgentId = std::string;
using TaskId = std::string;
using ModuleId = std::string;
using Timestamp = std::chrono::system_clock::time_point;

inline Timestamp now() {
    return std::chrono::system_clock::now();
}

inline std::string timestamp_to_string(const Timestamp ts) {
    const auto time_t = std::chrono::system_clock::to_time_t(ts);

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    char buf[64]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return std::string(buf);
}

inline std::string generate_uuid() {
    // 生成 RFC4122 v4 UUID。
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string uuid;
    uuid.reserve(36);
    for (int i = 0; i < 8; ++i) uuid += hex[dis(gen)];
    uuid += '-';
    for (int i = 0; i < 4; ++i) uuid += hex[dis(gen)];
    uuid += "-4";
    for (int i = 0; i < 3; ++i) uuid += hex[dis(gen)];
    uuid += '-';
    uuid += hex[8 + (dis(gen) & 0x03)];
    for (int i = 0; i < 3; ++i) uuid += hex[dis(gen)];
    uuid += '-';
    for (int i = 0; i < 12; ++i) uuid += hex[dis(gen)];
    return uuid;
}

} // namespace evoclaw
