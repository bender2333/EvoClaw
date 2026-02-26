# lib/bus — ZeroMQ 消息总线
add_library(evoclaw_bus OBJECT
  zmq_message_bus.cpp
)
target_include_directories(evoclaw_bus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(evoclaw_bus PUBLIC cppzmq_headers libzmq-static nlohmann_json::nlohmann_json)
