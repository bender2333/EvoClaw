# evoclaw-compiler
add_executable(evoclaw-compiler
  main.cpp
)
target_link_libraries(evoclaw-compiler PRIVATE evoclaw)
target_include_directories(evoclaw-compiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(evoclaw-compiler PRIVATE -Wall -Wextra -Wpedantic -Werror)
