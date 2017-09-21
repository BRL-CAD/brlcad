# Minimum required version of CMake
cmake_minimum_required(VERSION 3.0.2)

# Set CMake project name
project(TINYCTHREAD)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)

add_library(tinycthread SHARED tinycthread.c)
if(MSVC)
  # msvc does not append 'lib' - do it here to have consistent name
  set_target_properties(tinycthread PROPERTIES PREFIX "lib")
  set_target_properties(tinycthread PROPERTIES IMPORT_PREFIX "lib")
  set_property(TARGET tinycthread APPEND PROPERTY COMPILE_DEFINITIONS "TINYCTHREAD_DLL_EXPORTS")
endif(MSVC)
install(TARGETS tinycthread
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(BUILD_STATIC_LIBS)
  add_library(tinycthread-static STATIC tinycthread.c)
  if(MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(tinycthread-static PROPERTIES PREFIX "lib")
  else(MSVC)
    set_target_properties(tinycthread-static PROPERTIES OUTPUT_NAME "tinycthread")
  endif(MSVC)
  install(TARGETS tinycthread-static
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
