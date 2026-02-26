# ── 编译器版本检查 ──────────────────────────────────────────
# 要求 GCC 13+ 或 Clang 16+（std::expected 完整支持）
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.0")
    message(FATAL_ERROR "GCC 13+ required for full C++23 support (std::expected). Found: ${CMAKE_CXX_COMPILER_VERSION}")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16.0")
    message(FATAL_ERROR "Clang 16+ required for full C++23 support (std::expected). Found: ${CMAKE_CXX_COMPILER_VERSION}")
  endif()
else()
  message(WARNING "Untested compiler: ${CMAKE_CXX_COMPILER_ID}. GCC 13+ or Clang 16+ recommended.")
endif()

# ── 编译选项 ────────────────────────────────────────────────
add_compile_options(
  -Wall -Wextra -Wpedantic
  -Werror
  -Wno-unused-parameter      # 接口实现中常见未使用参数
  -Wshadow
  -Wnon-virtual-dtor
  -Woverloaded-virtual
  -Wcast-align
  -Wformat=2
)

# Debug 构建额外选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-g -O0 -fsanitize=address,undefined)
  add_link_options(-fsanitize=address,undefined)
endif()
