# Minimum required version of CMake
cmake_minimum_required(VERSION 3.0.2)

# Set CMake project name
project(RPLY)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)

add_library(rply rply.c)
if(MSVC)
  # msvc does not append 'lib' - do it here to have consistent name
  set_target_properties(rply PROPERTIES PREFIX "lib")
  set_target_properties(rply PROPERTIES IMPORT_PREFIX "lib")
  set_property(TARGET rply APPEND PROPERTY COMPILE_DEFINITIONS "RPLY_DLL_EXPORTS")
endif(MSVC)
install(TARGETS rply
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(BUILD_STATIC_LIBS)
  add_library(rply-static STATIC rply.c)
  if(MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(rply-static PROPERTIES PREFIX "lib")
  else(MSVC)
    set_target_properties(rply-static PROPERTIES OUTPUT_NAME "rply")
  endif(MSVC)
  install(TARGETS rply-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_STATIC_LIBS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
