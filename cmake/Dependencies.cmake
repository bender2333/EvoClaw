# ── 第三方依赖（FetchContent）──

# nlohmann/json
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

# yaml-cpp
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        0.8.0
  GIT_SHALLOW    TRUE
)
set(YAML_CPP_BUILD_TESTS OFF CACHE INTERNAL "")
set(YAML_CPP_BUILD_TOOLS OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(yaml-cpp)

# cpp-httplib
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(httplib)

# libzmq（从源码编译，不依赖系统安装）
FetchContent_Declare(
  libzmq
  GIT_REPOSITORY https://github.com/zeromq/libzmq.git
  GIT_TAG        v4.3.5
  GIT_SHALLOW    TRUE
)
set(ZMQ_BUILD_TESTS OFF CACHE INTERNAL "")
set(BUILD_TESTS OFF CACHE INTERNAL "")
set(BUILD_SHARED OFF CACHE INTERNAL "")
set(BUILD_STATIC ON CACHE INTERNAL "")
set(WITH_DOCS OFF CACHE INTERNAL "")
set(WITH_PERF_TOOL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(libzmq)

# cppzmq（仅获取源码，作为 header-only 使用）
FetchContent_Declare(
  cppzmq
  GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
  GIT_TAG        v4.10.0
  GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(cppzmq)
if(NOT cppzmq_POPULATED)
  FetchContent_Populate(cppzmq)
endif()
# 创建 header-only 接口库，不走 cppzmq 自带的 CMake（它会产生链接问题）
add_library(cppzmq_headers INTERFACE)
target_include_directories(cppzmq_headers INTERFACE ${cppzmq_SOURCE_DIR})

# GTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
  GIT_SHALLOW    TRUE
)
set(BUILD_GMOCK ON CACHE INTERNAL "")
set(INSTALL_GTEST OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(googletest)
