add_library(evoclaw_memory OBJECT
  fs_memory_store.cpp
)
target_include_directories(evoclaw_memory PUBLIC ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(evoclaw_memory PUBLIC nlohmann_json::nlohmann_json evoclaw_common)
