# ── lib/ 子模块 ──────────────────────────────────────────────
add_subdirectory(common)
add_subdirectory(config)
add_subdirectory(protocol)
add_subdirectory(bus)
add_subdirectory(event_log)
add_subdirectory(memory)
add_subdirectory(llm)

# ── 聚合为 libevoclaw.a ────────────────────────────────────
add_library(evoclaw STATIC)
target_link_libraries(evoclaw PUBLIC
  evoclaw_common
  evoclaw_config
  evoclaw_protocol
  evoclaw_bus
  evoclaw_event_log
  evoclaw_memory
  evoclaw_llm
)
target_include_directories(evoclaw PUBLIC ${CMAKE_SOURCE_DIR}/lib)
