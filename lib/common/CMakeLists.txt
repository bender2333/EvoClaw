# lib/common — header-only（error.h, thread_pool.h）
add_library(evoclaw_common INTERFACE)
target_include_directories(evoclaw_common INTERFACE ${CMAKE_SOURCE_DIR}/lib)
