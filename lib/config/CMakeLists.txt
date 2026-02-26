# lib/config — 配置管理
add_library(evoclaw_config OBJECT
  yaml_config.cpp
)
target_include_directories(evoclaw_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(evoclaw_config PUBLIC yaml-cpp::yaml-cpp)
