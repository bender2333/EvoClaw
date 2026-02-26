# evoclaw-engine
add_executable(evoclaw-engine
  main.cpp
  router/router.cpp
  router/capability_matrix.cpp
  governance/governance_gate.cpp
  governance/constitution.cpp
  governance/entropy_monitor.cpp
  safety/risk_classifier.cpp
  safety/permission_manager.cpp
  facade/facade.cpp
)
target_link_libraries(evoclaw-engine PRIVATE evoclaw)
target_include_directories(evoclaw-engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(evoclaw-engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
