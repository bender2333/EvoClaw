add_library(evoclaw_protocol OBJECT
  serializer.cpp
  basic_rule_engine.cpp
  cerebellum.cpp
)
target_include_directories(evoclaw_protocol PUBLIC ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(evoclaw_protocol PUBLIC nlohmann_json::nlohmann_json evoclaw_common)
