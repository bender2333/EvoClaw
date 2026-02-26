# ── 测试 ──
include(GoogleTest)

# 辅助函数：添加测试
function(evoclaw_add_test TEST_NAME TEST_SOURCE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(${TEST_NAME} PRIVATE evoclaw GTest::gtest_main GTest::gmock)
  target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib)
  gtest_discover_tests(${TEST_NAME})
endfunction()

# lib/ 单元测试
evoclaw_add_test(test_serializer lib/protocol/serializer_test.cpp)
evoclaw_add_test(test_basic_rule_engine lib/protocol/basic_rule_engine_test.cpp)
evoclaw_add_test(test_cerebellum lib/protocol/cerebellum_test.cpp)
evoclaw_add_test(test_jsonl_event_store lib/event_log/jsonl_event_store_test.cpp)
evoclaw_add_test(test_event_merger lib/event_log/event_merger_test.cpp)
evoclaw_add_test(test_fs_memory_store lib/memory/fs_memory_store_test.cpp)
evoclaw_add_test(test_yaml_config lib/config/yaml_config_test.cpp)
evoclaw_add_test(test_prompt_registry lib/llm/prompt_registry_test.cpp)

# 集成测试
evoclaw_add_test(test_bus_integration integration/bus_integration_test.cpp)

# src/engine/ 组件测试（需要 engine 源码目录的 include path）
function(evoclaw_add_engine_test TEST_NAME TEST_SOURCE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(${TEST_NAME} PRIVATE evoclaw GTest::gtest_main GTest::gmock)
  target_include_directories(${TEST_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/src/engine
  )
  gtest_discover_tests(${TEST_NAME})
endfunction()

# Router 测试需要链接 router 源文件
add_executable(test_router
  src/engine/router_test.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/router/router.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/router/capability_matrix.cpp
)
target_link_libraries(test_router PRIVATE evoclaw GTest::gtest_main GTest::gmock)
target_include_directories(test_router PRIVATE
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_SOURCE_DIR}/src/engine
)
gtest_discover_tests(test_router)

# Governance 测试
add_executable(test_governance
  src/engine/governance_test.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/governance/governance_gate.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/governance/constitution.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/governance/entropy_monitor.cpp
)
target_link_libraries(test_governance PRIVATE evoclaw GTest::gtest_main GTest::gmock)
target_include_directories(test_governance PRIVATE
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_SOURCE_DIR}/src/engine
)
gtest_discover_tests(test_governance)

# Safety 测试
add_executable(test_safety
  src/engine/safety_test.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/safety/risk_classifier.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/safety/permission_manager.cpp
)
target_link_libraries(test_safety PRIVATE evoclaw GTest::gtest_main GTest::gmock)
target_include_directories(test_safety PRIVATE
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_SOURCE_DIR}/src/engine
)
gtest_discover_tests(test_safety)

# Facade 测试
add_executable(test_facade
  src/engine/facade_test.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/facade/facade.cpp
)
target_link_libraries(test_facade PRIVATE evoclaw GTest::gtest_main GTest::gmock)
target_include_directories(test_facade PRIVATE
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_SOURCE_DIR}/src/engine
)
gtest_discover_tests(test_facade)
