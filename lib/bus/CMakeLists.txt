add_library(evoclaw_bus OBJECT
  zmq_message_bus.cpp
)
target_include_directories(evoclaw_bus PUBLIC ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(evoclaw_bus PUBLIC cppzmq evoclaw_common evoclaw_protocol)
target_include_directories(evoclaw_bus PUBLIC ${ZMQ_INCLUDE_DIRS})
target_link_libraries(evoclaw_bus PUBLIC ${ZMQ_LIBRARIES})
