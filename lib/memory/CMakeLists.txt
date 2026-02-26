# lib/memory — 记忆存储
add_library(evoclaw_memory OBJECT
  fs_memory_store.cpp
)
target_include_directories(evoclaw_memory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(evoclaw_memory PUBLIC nlohmann_json::nlohmann_json)
