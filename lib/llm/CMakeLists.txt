# lib/llm — LLM 客户端
add_library(evoclaw_llm OBJECT
  openai_client.cpp
  prompt_registry.cpp
)
target_include_directories(evoclaw_llm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(evoclaw_llm PUBLIC
  nlohmann_json::nlohmann_json
  yaml-cpp::yaml-cpp
  httplib::httplib
)
