# evoclaw-dashboard
add_executable(evoclaw-dashboard
  main.cpp
)
target_link_libraries(evoclaw-dashboard PRIVATE evoclaw)
target_compile_options(evoclaw-dashboard PRIVATE -Wall -Wextra -Wpedantic -Werror)
