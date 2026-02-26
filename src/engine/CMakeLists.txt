add_executable(evoclaw-engine main.cpp)
target_link_libraries(evoclaw-engine PRIVATE evoclaw dpp spdlog::spdlog)
