find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()
include(GoogleTest)

add_executable(evoclaw_test
    test_core.cpp
    test_protocol.cpp
    test_umi.cpp
    test_agent.cpp
    test_router.cpp
    test_memory.cpp
    test_event_log.cpp
    test_governance.cpp
    test_evolution.cpp
)

target_link_libraries(evoclaw_test PRIVATE evoclaw_core GTest::gtest_main)
gtest_discover_tests(evoclaw_test)
