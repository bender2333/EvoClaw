# Phase 1 Implementation Task

## 目标
实现 EvoClaw C++ 核心基础设施：Core 类型、Protocol 消息系统、UMI 契约。

## 需要创建的文件

### 1. CMakeLists.txt（根目录）
```cmake
cmake_minimum_required(VERSION 3.25)
project(EvoClaw VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译选项
add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
else()
    add_compile_options(-O3)
endif()

# 依赖
find_package(Threads REQUIRED)

# 第三方库（单头文件，直接包含）
include_directories(third_party)

# 核心库
file(GLOB_RECURSE EVOLCLAW_SOURCES src/*.cpp)
add_library(evoclaw_core ${EVOLCLAW_SOURCES})
target_include_directories(evoclaw_core PUBLIC src)
target_link_libraries(evoclaw_core PUBLIC Threads::Threads)

# 测试
enable_testing()
add_subdirectory(tests)

# 示例
add_subdirectory(examples)
```

### 2. third_party/json.hpp
下载 nlohmann/json 单头文件：https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp

### 3. src/core/types.hpp
```cpp
#pragma once

#include <chrono>
#include <string>
#include <optional>
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

inline std::string timestamp_to_string(Timestamp ts) {
    auto time_t = std::chrono::system_clock::to_time_t(ts);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&time_t));
    return std::string(buf);
}

inline std::string generate_uuid() {
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
```

### 4. src/protocol/message.hpp
```cpp
#pragma once

#include "core/types.hpp"
#include <nlohmann/json.hpp>
#include <optional>
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
    Performative performative;
    AgentId sender;
    std::vector<AgentId> receivers;
    nlohmann::json payload;
    nlohmann::json metadata;
    std::optional<MessageId> parent_id;
    
    Message() : id(generate_uuid()), metadata(nlohmann::json::object()) {}
    
    [[nodiscard]] std::string to_json() const {
        return nlohmann::json(*this).dump(2);
    }
    
    [[nodiscard]] static std::optional<Message> from_json(const std::string& json_str) {
        try {
            return nlohmann::json::parse(json_str).get<Message>();
        } catch (...) {
            return std::nullopt;
        }
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Message, id, performative, sender, receivers, payload, metadata, parent_id)

} // namespace evoclaw::protocol
```

### 5. src/protocol/bus.hpp 和 bus.cpp
消息总线实现（支持订阅、发送、广播、去重、限流）。

### 6. src/umi/contract.hpp
UMI 契约定义（CapabilityProfile, AirbagSpec, ModuleContract）。

### 7. src/umi/validator.hpp
契约验证器。

### 8. tests/CMakeLists.txt
```cmake
find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(evoclaw_test
    test_core.cpp
    test_protocol.cpp
    test_umi.cpp
)

target_link_libraries(evoclaw_test PRIVATE evoclaw_core GTest::gtest_main)
gtest_discover_tests(evoclaw_test)
```

### 9. tests/test_core.cpp
测试 UUID 生成、时间戳转换。

### 10. tests/test_protocol.cpp
测试 Message 序列化/反序列化、Performative 枚举。

### 11. tests/test_umi.cpp
测试 ModuleContract 验证。

## 验收标准
1. `mkdir build && cd build && cmake .. && make -j` 编译成功
2. `./tests/evoclaw_test` 所有测试通过
3. 示例程序能创建 Message 并序列化/反序列化

---
开始实现。保持代码现代 C++ 风格（C++23），完整类型注解，中文注释。
