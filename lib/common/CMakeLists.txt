# lib/common — 通用工具
add_library(evoclaw_common OBJECT
  uuid.cpp
)
target_include_directories(evoclaw_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
