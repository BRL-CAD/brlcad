# Minimum required version of CMake
cmake_minimum_required(VERSION 3.1.3)

# Set CMake project name
project(TINYCTHREAD)

# Match BRL-CAD
unset(C_STANDARD_FLAGS)
set(CMAKE_C_EXTENSIONS OFF)
if("${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND NOT MSVC)
  set(CMAKE_C_STANDARD 99)
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(API_FLAGS "-D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=500")
  if(CMAKE_C_EXTENSIONS)
    set(C_STANDARD_FLAGS "${CMAKE_C${CMAKE_C_STANDARD}_EXTENSION_COMPILE_OPTION}")
  else()
    set(C_STANDARD_FLAGS "${CMAKE_C${CMAKE_C_STANDARD}_STANDARD_COMPILE_OPTION} ${API_FLAGS}")
  endif(CMAKE_C_EXTENSIONS)
  if("${C_STANDARD_FLAGS}" STREQUAL "")
    message(FATAL_ERROR "C standard specification failure: compiler does not support the C${CMAKE_C_STANDARD} standard:\n${CMAKE_C_COMPILE_FEATURES}")
  endif("${C_STANDARD_FLAGS}" STREQUAL "")
else()
  set(CMAKE_C_STANDARD 90)
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(API_FLAGS "-D_POSIX_C_SOURCE=199009L -D_XOPEN_SOURCE=3")
  if(CMAKE_C_EXTENSIONS)
    set(C_STANDARD_FLAGS "${CMAKE_C${CMAKE_C_STANDARD}_EXTENSION_COMPILE_OPTION}")
  else()
    set(C_STANDARD_FLAGS "${CMAKE_C${CMAKE_C_STANDARD}_STANDARD_COMPILE_OPTION} ${API_FLAGS}")
  endif(CMAKE_C_EXTENSIONS)
endif("${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND NOT MSVC)

include(CheckStructHasMember)
cmake_push_check_state()
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${CMAKE_C_STD_FLAG}")
check_struct_has_member("struct timespec" tv_sec "time.h;sys/select.h"  HAVE_STRUCT_TIMESPEC)
cmake_pop_check_state()
if(HAVE_STRUCT_TIMESPEC)
  add_definitions(-DHAVE_STRUCT_TIMESPEC=1)
endif(HAVE_STRUCT_TIMESPEC)

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
