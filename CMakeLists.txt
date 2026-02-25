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
set(EVOLCLAW_SOURCES
    src/agent/agent.cpp
    src/agent/critic.cpp
    src/agent/executor.cpp
    src/agent/planner.cpp
    src/event_log/event_log.cpp
    src/evolution/evolver.cpp
    src/facade/facade.cpp
    src/governance/governance_kernel.cpp
    src/memory/org_log.cpp
    src/memory/working_memory.cpp
    src/protocol/bus.cpp
    src/router/router.cpp
)
add_library(evoclaw_core ${EVOLCLAW_SOURCES})
target_include_directories(evoclaw_core PUBLIC src)
target_link_libraries(evoclaw_core PUBLIC Threads::Threads)

# 测试
enable_testing()
add_subdirectory(tests)

# 示例
add_subdirectory(examples)
