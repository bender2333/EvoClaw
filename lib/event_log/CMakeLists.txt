add_library(evoclaw_event_log OBJECT
  jsonl_event_store.cpp
  event_merger.cpp
)
target_include_directories(evoclaw_event_log PUBLIC ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(evoclaw_event_log PUBLIC nlohmann_json::nlohmann_json evoclaw_common evoclaw_protocol)
