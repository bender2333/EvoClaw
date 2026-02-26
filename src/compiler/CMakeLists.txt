add_executable(evoclaw-compiler main.cpp)
target_link_libraries(evoclaw-compiler PRIVATE evoclaw spdlog::spdlog)
