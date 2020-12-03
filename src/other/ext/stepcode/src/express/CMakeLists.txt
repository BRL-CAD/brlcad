include_directories(
  ${CMAKE_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SC_SOURCE_DIR}/src/base
  )

# Depending on whether we're using pre-generated sources or building them on
# the fly, set up targets and source lists.
if(SC_GENERATE_LP_SOURCES)
  LEMON_TARGET(ExpParser expparse.y)
  PERPLEX_TARGET(ExpScanner expscan.l)
  ADD_PERPLEX_LEMON_DEPENDENCY(ExpScanner ExpParser)
  include_directories(
    ${LEMON_ExpParser_INCLUDE_DIR}
    ${PERPLEX_ExpScanner_INCLUDE_DIR}
    )
else(SC_GENERATE_LP_SOURCES)
  set(LEMON_ExpParser_SRC ${CMAKE_CURRENT_SOURCE_DIR}/generated/expparse.c)
  set(LEMON_ExpParser_HDR ${CMAKE_CURRENT_SOURCE_DIR}/generated/expparse.h)
  set(PERPLEX_ExpScanner_SRC ${CMAKE_CURRENT_SOURCE_DIR}/generated/expscan.c)
  set(PERPLEX_ExpScanner_HDR ${CMAKE_CURRENT_SOURCE_DIR}/generated/expscan.h)
  include_directories(generated)
endif(SC_GENERATE_LP_SOURCES)

set(EXPRESS_SOURCES
  ${LEMON_ExpParser_HDR}
  ${LEMON_ExpParser_SRC}
  ${PERPLEX_ExpScanner_HDR}
  ${PERPLEX_ExpScanner_SRC}
  symbol.c
  type.c
  variable.c
  expr.c
  entity.c
  caseitem.c
  stmt.c
  alg.c
  scope.c
  schema.c
  resolve.c
  resolve2.c
  lexact.c
  linklist.c
  error.c
  dict.c
  hash.c
  alloc.c
  memory.c
  object.c
  express.c
  ordered_attrs.cc
  info.c
  factory.c
  )

add_library(express-obj OBJECT ${EXPRESS_SOURCES})
add_dependencies(express-obj base)
set_property(TARGET express-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  set_property(TARGET express-obj APPEND PROPERTY COMPILE_DEFINITIONS "SC_EXPRESS_DLL_EXPORTS")
endif(MSVC)

add_library(express SHARED $<TARGET_OBJECTS:express-obj>)
target_link_libraries(express base)
if(OPENBSD)
  set_target_properties(express PROPERTIES VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})
else(OPENBSD)
  set_target_properties(express PROPERTIES VERSION ${SC_VERSION} SOVERSION ${SC_VERSION_MAJOR})
endif(OPENBSD)
if(APPLE)
  set_property(TARGET express APPEND PROPERTY LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
if(MSVC)
  set_property(TARGET express APPEND PROPERTY COMPILE_DEFINITIONS "SC_EXPRESS_DLL_EXPORTS")
  set_property(TARGET express APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_EXPRESS_DLL_IMPORTS")
endif(MSVC)
install(TARGETS express
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (BUILD_STATIC_LIBS)
  add_library(express-static STATIC $<TARGET_OBJECTS:express-obj>)
  install(TARGETS express-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

# TODO
# Currently, fedex.c provides the main() for multiple programs. These programs
# provide custom behavior by defining EXPRESSinit_init (called by fedex.c's
# main) which sets other callbacks.
#
# What would be better is to remove the call to EXPRESSinit_init and turn
# fedex.c's main() into an express library function. That way, programs only
# need to link against express (not fedex.o too) and they can do initialization
# in their own main (instead of in a local EXPRESSinit_init definition).
set(CHECK_EXPRESS_SOURCES
  fedex.c
  inithook.c
 )
add_executable(check-express ${CHECK_EXPRESS_SOURCES})
target_link_libraries(check-express express base)

if(SC_ENABLE_TESTING)
  add_subdirectory(test)
endif(SC_ENABLE_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
