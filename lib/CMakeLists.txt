# ── libevoclaw: 聚合所有 OBJECT 库 ──

add_subdirectory(common)
add_subdirectory(config)
add_subdirectory(protocol)
add_subdirectory(bus)
add_subdirectory(event_log)
add_subdirectory(memory)
add_subdirectory(llm)

# 聚合为静态库
add_library(evoclaw STATIC
  $<TARGET_OBJECTS:evoclaw_common>
  $<TARGET_OBJECTS:evoclaw_config>
  $<TARGET_OBJECTS:evoclaw_protocol>
  $<TARGET_OBJECTS:evoclaw_bus>
  $<TARGET_OBJECTS:evoclaw_event_log>
  $<TARGET_OBJECTS:evoclaw_memory>
  $<TARGET_OBJECTS:evoclaw_llm>
)

target_link_libraries(evoclaw PUBLIC
  nlohmann_json::nlohmann_json
  yaml-cpp::yaml-cpp
  httplib::httplib
  cppzmq_headers
  libzmq-static
)

target_include_directories(evoclaw PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
# 对所有项目自身目标设置严格编译选项
set(EVOCLAW_TARGETS evoclaw evoclaw_common evoclaw_config evoclaw_protocol evoclaw_bus evoclaw_event_log evoclaw_memory evoclaw_llm)
foreach(tgt ${EVOCLAW_TARGETS})
  target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endforeach()
