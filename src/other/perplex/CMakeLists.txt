CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

PROJECT(PERPLEX)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(PERPLEX_CMAKE_DIR "${PERPLEX_SOURCE_DIR}/CMake")
if(PERPLEX_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${PERPLEX_CMAKE_DIR}")
else(PERPLEX_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${PERPLEX_CMAKE_DIR};${CMAKE_MODULE_PATH}")
endif(PERPLEX_IS_SUBBUILD)

FIND_PACKAGE(LEMON)
FIND_PACKAGE(RE2C)

if(NOT LEMON_EXECUTABLE OR NOT RE2C_EXECUTABLE)
  message(FATAL_ERROR "Perplex requires the LEMON Parser Generator and the re2c scanner generator.")
endif(NOT LEMON_EXECUTABLE OR NOT RE2C_EXECUTABLE)

if(NOT COMMAND LEMON_TARGET)
  include(${PERPLEX_CMAKE_DIR}/LEMON_Util.cmake)
endif(NOT COMMAND LEMON_TARGET)
if(NOT COMMAND RE2C_TARGET)
  include(${PERPLEX_CMAKE_DIR}/RE2C_Util.cmake)
endif(NOT COMMAND RE2C_TARGET)

RE2C_TARGET(PERPLEX scanner.re ${CMAKE_CURRENT_BINARY_DIR}/scanner.c COMPILE_FLAGS "-c")
LEMON_TARGET(PERPLEX parser.y parser.c parser.h)
ADD_RE2C_LEMON_DEPENDENCY(PERPLEX PERPLEX)

set(perplex_sources
  ${LEMON_PERPLEX_OUTPUT_SOURCE}
  ${RE2C_PERPLEX_OUTPUTS}
  mbo_getopt.cpp
  perplex.cpp
  )

add_executable(perplex ${perplex_sources})

# Need to link against stl library with some compilers.
target_link_libraries(perplex "${STDCXX_LIBRARIES}")

# Install
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT PERPLEX_SKIP_INSTALL)
  install(TARGETS perplex DESTINATION ${BIN_DIR})
endif(NOT PERPLEX_SKIP_INSTALL)

# Handle Perplex template
if(NOT DATA_DIR)
  set(DATA_DIR share/perplex)
endif(NOT DATA_DIR)

if(NOT CMAKE_CONFIGURATION_TYPES)
  set(PERPLEX_TEMPLATE_DIR ${CMAKE_BINARY_DIR}/${DATA_DIR})
else(NOT CMAKE_CONFIGURATION_TYPES)
  set(PERPLEX_TEMPLATE_DIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${DATA_DIR})
endif(NOT CMAKE_CONFIGURATION_TYPES)
add_custom_command(
  OUTPUT ${PERPLEX_TEMPLATE_DIR}/perplex_template.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/perplex_template.c ${PERPLEX_TEMPLATE_DIR}/perplex_template.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/perplex_template.c
  )
add_custom_target(perplex_template_cp ALL DEPENDS ${PERPLEX_TEMPLATE_DIR}/perplex_template.c)
add_dependencies(perplex perplex_template_cp)

if(NOT PERPLEX_SKIP_INSTALL)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/perplex_template.c DESTINATION ${DATA_DIR})
endif(NOT PERPLEX_SKIP_INSTALL)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
