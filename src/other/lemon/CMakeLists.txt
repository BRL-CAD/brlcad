# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# Set CMake project name
PROJECT(LEMON)

add_executable(lemon lemon.c)

# make sure our list of outputs is clear
set(lempar_cp_outputs)

if(NOT CMAKE_CONFIGURATION_TYPES)
  get_target_property(LEMPAR_DIR lemon RUNTIME_OUTPUT_DIRECTORY)
  add_custom_command(
    OUTPUT ${LEMPAR_DIR}/lempar.c
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c ${LEMPAR_DIR}/lempar.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c
    )
  add_custom_target(lempar_cp ALL DEPENDS ${LEMPAR_DIR}/lempar.c)
else(NOT CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    get_target_property(LEMPAR_DIR lemon RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER})
    add_custom_command(
      OUTPUT ${LEMPAR_DIR}/lempar.c
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c ${LEMPAR_DIR}/lempar.c
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c
      )
    set(lempar_cp_outputs ${lempar_cp_outputs} ${LEMPAR_DIR}/lempar.c)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  add_custom_target(lempar_cp ALL DEPENDS ${lempar_cp_outputs})
endif(NOT CMAKE_CONFIGURATION_TYPES)

add_dependencies(lemon lempar_cp)

