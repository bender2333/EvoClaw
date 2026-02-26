add_library(evoclaw_llm OBJECT
  openai_client.cpp
  prompt_registry.cpp
)
target_include_directories(evoclaw_llm PUBLIC ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(evoclaw_llm PUBLIC httplib::httplib nlohmann_json::nlohmann_json evoclaw_common)
