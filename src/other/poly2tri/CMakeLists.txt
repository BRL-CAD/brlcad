# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
endif(COMMAND CMAKE_POLICY)

# Set CMake project name
project(Poly2Tri)

# SET LIBNAME
set(P2T_LIB_NAME p2t)

# Testing option
if(NOT DEFINED P2T_TESTS)
  set(P2T_TESTS OFF)
endif(NOT DEFINED P2T_TESTS)
OPTION(P2T_TESTS "Build libp2t tests" ${P2T_TESTS})
mark_as_advanced(P2T_TESTS)
if(P2T_TESTS)
  include(CTest)
  enable_testing()
endif(P2T_TESTS)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(LIBP2T_PUBLIC_HDRS
  poly2tri/poly2tri.h
  poly2tri/common/shapes.h
  poly2tri/sweep/cdt.h
  )
set(LIBP2T_PRIVATE_HDRS
  poly2tri/common/utils.h
  poly2tri/sweep/advancing_front.h
  poly2tri/sweep/sweep.h
  poly2tri/sweep/sweep_context.h
  )
set(LIBP2T_SOURCES
  poly2tri/common/shapes.cc
  poly2tri/common/utils.cc
  poly2tri/sweep/advancing_front.cc
  poly2tri/sweep/cdt.cc
  poly2tri/sweep/sweep_context.cc
  poly2tri/sweep/sweep.cc
  )
set(LIBP2T_TEST_SOURCES
  testbed/main.cc
  )

if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)

if(MSVC)
  add_definitions("-DP2T_DLL_EXPORTS")
endif(MSVC)
add_definitions("-DP2T_NO_GLFW")

add_library(${P2T_LIB_NAME} ${LIBP2T_SOURCES} ${LIBP2T_PUBLIC_HDRS} ${LIBP2T_PRIVATE_HDRS})
set_target_properties(${P2T_LIB_NAME} PROPERTIES VERSION 1.0.1 SOVERSION 1)
if(MSVC)
  # msvc does not append 'lib' - do it here to have consistent name
  set_target_properties(${P2T_LIB_NAME} PROPERTIES PREFIX "lib")
  set_target_properties(${P2T_LIB_NAME} PROPERTIES IMPORT_PREFIX "lib")
endif()
target_link_libraries(${P2T_LIB_NAME} ${ZLIB_LIBRARY} ${M_LIBRARY})
install(TARGETS ${P2T_LIB_NAME}
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(BUILD_STATIC_LIBS)
  set(P2T_LIB_NAME_STATIC ${P2T_LIB_NAME}_static)
  add_library(${P2T_LIB_NAME_STATIC} STATIC ${LIBP2T_SOURCES})
  if(MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(${P2T_LIB_NAME_STATIC} PROPERTIES PREFIX "lib")
  endif()

  if(BRLCAD_ENABLE_BRLCAD_LIBRARY AND ${CMAKE_PROJECT_NAME}_ZLIB_BUILD AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
    target_link_libraries(${P2T_LIB_NAME_STATIC} zlib-static ${M_LIBRARY})
  else(BRLCAD_ENABLE_BRLCAD_LIBRARY AND ${CMAKE_PROJECT_NAME}_ZLIB_BUILD AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
    target_link_libraries(${P2T_LIB_NAME_STATIC} ${ZLIB_LIBRARY} ${M_LIBRARY})
  endif(BRLCAD_ENABLE_BRLCAD_LIBRARY AND ${CMAKE_PROJECT_NAME}_ZLIB_BUILD AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)

  install(TARGETS ${P2T_LIB_NAME_STATIC}
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_STATIC_LIBS)

if(WIN32)
  set_target_properties(${P2T_LIB_NAME} PROPERTIES DEFINE_SYMBOL P2T_BUILD_DLL)
endif()

##if(P2T_TESTS)
##  if(P2T_STATIC)
##    add_executable(p2t-test ${LIBP2T_TEST_SOURCES})
##    target_link_libraries(p2t-test ${P2T_LIB_NAME_STATIC})
##    add_test(p2t-test ./p2t-test ${CMAKE_CURRENT_SOURCE_DIR}/DATA/bird.dat)
##  else()
##    add_executable(p2t-test ${LIBP2T_TEST_SOURCES})
##    target_link_libraries(p2t-test ${P2T_LIB_NAME})
##    add_test(p2t-test ./p2t-test ${CMAKE_CURRENT_SOURCE_DIR}/DATA/bird.dat)
##  endif()
##endif()

install(FILES poly2tri/poly2tri.h DESTINATION include/poly2tri)
install(FILES poly2tri/common/shapes.h DESTINATION include/poly2tri/common)
install(FILES poly2tri/sweep/cdt.h DESTINATION include/poly2tri/sweep)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
