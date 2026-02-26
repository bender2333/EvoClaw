include(GoogleTest)

# ── lib/ 单元测试 ───────────────────────────────────────────

# common — 空测试（验证构建系统）
add_executable(common_test lib/common/common_test.cpp)
target_link_libraries(common_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(common_test)

# config
add_executable(yaml_config_test lib/config/yaml_config_test.cpp)
target_link_libraries(yaml_config_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(yaml_config_test)

# protocol
add_executable(serializer_test lib/protocol/serializer_test.cpp)
target_link_libraries(serializer_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(serializer_test)

add_executable(basic_rule_engine_test lib/protocol/basic_rule_engine_test.cpp)
target_link_libraries(basic_rule_engine_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(basic_rule_engine_test)

# bus
add_executable(zmq_message_bus_test lib/bus/zmq_message_bus_test.cpp)
target_link_libraries(zmq_message_bus_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(zmq_message_bus_test)

# event_log
add_executable(jsonl_event_store_test lib/event_log/jsonl_event_store_test.cpp)
target_link_libraries(jsonl_event_store_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(jsonl_event_store_test)

add_executable(event_merger_test lib/event_log/event_merger_test.cpp)
target_link_libraries(event_merger_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(event_merger_test)

# memory
add_executable(fs_memory_store_test lib/memory/fs_memory_store_test.cpp)
target_link_libraries(fs_memory_store_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(fs_memory_store_test)

# llm
add_executable(openai_client_test lib/llm/openai_client_test.cpp)
target_link_libraries(openai_client_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(openai_client_test)

add_executable(prompt_registry_test lib/llm/prompt_registry_test.cpp)
target_link_libraries(prompt_registry_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(prompt_registry_test)

# ── 集成测试 ────────────────────────────────────────────────

add_executable(bus_integration_test integration/bus_integration_test.cpp)
target_link_libraries(bus_integration_test PRIVATE evoclaw GTest::gtest_main)
gtest_discover_tests(bus_integration_test)
