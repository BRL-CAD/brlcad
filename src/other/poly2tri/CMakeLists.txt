# Minimum required version of CMake
cmake_minimum_required(VERSION 3.1.3)

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
  poly2tri/sweep/advancing_front.h
  poly2tri/sweep/sweep.h
  poly2tri/sweep/sweep_context.h
  )

set(LIBP2T_PRIVATE_HDRS
  poly2tri/common/utils.h
  )

set(LIBP2T_SOURCES
  poly2tri/common/shapes.cc
  poly2tri/common/utils.cc
  poly2tri/sweep/advancing_front.cc
  poly2tri/sweep/cdt.cc
  poly2tri/sweep/sweep_context.cc
  poly2tri/sweep/sweep.cc
  )

if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)

add_definitions("-DP2T_NO_GLFW")

add_library(${P2T_LIB_NAME} ${LIBP2T_SOURCES} ${LIBP2T_PUBLIC_HDRS} ${LIBP2T_PRIVATE_HDRS})
set_target_properties(${P2T_LIB_NAME} PROPERTIES VERSION 1.0.1 SOVERSION 1)
if(MSVC)
  # msvc does not append 'lib' - do it here to have consistent name
  set_target_properties(${P2T_LIB_NAME} PROPERTIES PREFIX "lib")
  set_target_properties(${P2T_LIB_NAME} PROPERTIES IMPORT_PREFIX "lib")
  set_property(TARGET ${P2T_LIB_NAME} APPEND PROPERTY COMPILE_DEFINITIONS "P2T_DLL_EXPORTS")
endif()
target_link_libraries(${P2T_LIB_NAME} ${ZLIB_LIBRARY} ${M_LIBRARY})
install(TARGETS ${P2T_LIB_NAME}
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(BUILD_STATIC_LIBS)
  set(P2T_LIB_NAME_STATIC ${P2T_LIB_NAME}-static)
  add_library(${P2T_LIB_NAME_STATIC} STATIC ${LIBP2T_SOURCES})
  if(MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(${P2T_LIB_NAME_STATIC} PROPERTIES PREFIX "lib")
  else(MSVC)
    set_target_properties(${P2T_LIB_NAME_STATIC} PROPERTIES OUTPUT_NAME "${P2T_LIB_NAME}")
  endif(MSVC)

  install(TARGETS ${P2T_LIB_NAME_STATIC}
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_STATIC_LIBS)

if(WIN32)
  set_target_properties(${P2T_LIB_NAME} PROPERTIES DEFINE_SYMBOL P2T_BUILD_DLL)
endif()

install(FILES poly2tri/poly2tri.h DESTINATION include/poly2tri)
install(FILES poly2tri/common/shapes.h DESTINATION include/poly2tri/common)
install(FILES poly2tri/sweep/cdt.h DESTINATION include/poly2tri/sweep)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
