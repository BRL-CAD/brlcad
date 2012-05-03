CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

PROJECT(PERPLEX)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

FIND_PACKAGE(LEMON)
FIND_PACKAGE(RE2C)

RE2C_TARGET(PERPLEX scanner.re ${CMAKE_CURRENT_BINARY_DIR}/scanner.c
  COMPILE_FLAGS "-c")
LEMON_TARGET(PERPLEX parser.y parser.c parser.h)
ADD_RE2C_LEMON_DEPENDENCY(PERPLEX PERPLEX)

set(perplex_sources
  ${LEMON_PERPLEX_OUTPUT_SOURCE}
  ${RE2C_PERPLEX_OUTPUTS}
  mbo_getopt.cpp
  perplex.cpp
  )

add_executable(perplex ${perplex_sources})

# FIXME: need to link against stl library with some compilers.
# assuming libstdc++ but not portable, need to test for it.
target_link_libraries(perplex stdc++)
