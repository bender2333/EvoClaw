# lib/protocol — 消息协议 + Cortex Layer II/III
add_library(evoclaw_protocol OBJECT
  serializer.cpp
  basic_rule_engine.cpp
  cerebellum.cpp
)
target_include_directories(evoclaw_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(evoclaw_protocol PUBLIC nlohmann_json::nlohmann_json)
