add_library(evoclaw_config OBJECT
  yaml_config.cpp
)
target_include_directories(evoclaw_config PUBLIC ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(evoclaw_config PUBLIC yaml-cpp evoclaw_common)
