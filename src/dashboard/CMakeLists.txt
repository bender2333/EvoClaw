add_executable(evoclaw-dashboard main.cpp)
target_link_libraries(evoclaw-dashboard PRIVATE evoclaw httplib::httplib spdlog::spdlog)
