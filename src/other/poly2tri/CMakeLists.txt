# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# Set CMake project name
PROJECT(Poly2Tri)
##enable_testing()

# SET LIBNAME
set(P2T_LIB_NAME p2t)

# Testing option
OPTION_WITH_DEFAULT(P2T_TESTS  "Build libp2t tests" NO)
MARK_AS_ADVANCED(P2T_TESTS)
IF(P2T_TESTS)
  INCLUDE(CTest)
  ENABLE_TESTING()
ENDIF(P2T_TESTS)

INCLUDE_DIRECTORIES(
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

IF(NOT LIB_DIR)
    SET(LIB_DIR lib)
ENDIF(NOT LIB_DIR)
IF(NOT BIN_DIR)
    SET(BIN_DIR bin)
ENDIF(NOT BIN_DIR)

IF(MSVC)
  add_definitions("-DP2T_DLL_EXPORTS")
ENDIF(MSVC)
add_definitions("-DP2T_NO_GLFW")

add_library(${P2T_LIB_NAME} ${LIBP2T_SOURCES} ${LIBP2T_PUBLIC_HDRS} ${LIBP2T_PRIVATE_HDRS})
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
  target_link_libraries(${P2T_LIB_NAME_STATIC} ${ZLIB_LIBRARY} ${M_LIBRARY})
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













