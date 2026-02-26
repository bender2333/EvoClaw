cmake_minimum_required(VERSION 3.25)
project(EvoClaw
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "AI 管家系统 — 组织进化架构"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── CMake 模块 ──────────────────────────────────────────────
include(cmake/compiler_options.cmake)
include(cmake/dependencies.cmake)

# ── 共享库 libevoclaw ───────────────────────────────────────
add_subdirectory(lib)

# ── 可执行文件 ──────────────────────────────────────────────
add_subdirectory(src/engine)
add_subdirectory(src/compiler)
add_subdirectory(src/dashboard)
add_subdirectory(src/judge)

# ── 测试 ────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
