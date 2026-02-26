# evoclaw-judge (Phase 2 stub)
add_executable(evoclaw-judge
  main.cpp
)
target_link_libraries(evoclaw-judge PRIVATE evoclaw)
target_compile_options(evoclaw-judge PRIVATE -Wall -Wextra -Wpedantic -Werror)
