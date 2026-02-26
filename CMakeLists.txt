cmake_minimum_required(VERSION 3.25)
project(EvoClaw VERSION 0.1.0 LANGUAGES CXX)

# ── C++23 标准 ──
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── 编译选项（仅对项目自身代码生效）──
# 不在全局设置 -Werror，避免影响 FetchContent 第三方库

# ── FetchContent 依赖 ──
include(FetchContent)
include(cmake/Dependencies.cmake)

# ── 共享库 ──
# 对项目自身目标统一设置严格编译选项
add_subdirectory(lib)

# ── 可执行文件 ──
add_subdirectory(src/engine)
add_subdirectory(src/compiler)
add_subdirectory(src/judge)
add_subdirectory(src/dashboard)

# ── 测试 ──
option(EVOCLAW_BUILD_TESTS "构建测试" ON)
if(EVOCLAW_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
