# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# Set CMake project name
PROJECT(LEMON)

add_executable(lemon lemon.c)
# Install
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT LEMON_SKIP_INSTALL)
  install(TARGETS lemon DESTINATION ${BIN_DIR})
endif(NOT LEMON_SKIP_INSTALL)

# Handle lempar.c template file
if(NOT CMAKE_CONFIGURATION_TYPES)
  set(LEMPAR_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
else(NOT CMAKE_CONFIGURATION_TYPES)
  set(LEMPAR_DIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${BIN_DIR})
endif(NOT CMAKE_CONFIGURATION_TYPES)
add_custom_command(
  OUTPUT ${LEMPAR_DIR}/lempar.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c ${LEMPAR_DIR}/lempar.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c
  )
add_custom_target(lempar_cp ALL DEPENDS ${LEMPAR_DIR}/lempar.c)
add_dependencies(lemon lempar_cp)

if(NOT LEMON_SKIP_INSTALL)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c DESTINATION ${BIN_DIR})
endif(NOT LEMON_SKIP_INSTALL)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
