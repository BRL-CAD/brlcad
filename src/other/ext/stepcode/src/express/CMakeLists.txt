include_directories(
  ${CMAKE_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

# Depending on whether we're using pre-generated sources or building them on
# the fly, set up targets and source lists.
if(SC_GENERATE_LP_SOURCES)
  LEMON_TARGET(ExpParser expparse.y expparse.c
    COMPILE_FLAGS "-p -c"
    DEFINES_FILE "expparse.h"
    )
  PERPLEX_TARGET(ExpScanner expscan.l)
  ADD_PERPLEX_LEMON_DEPENDENCY(ExpScanner ExpParser)

  add_library(objlib_expscan_c OBJECT ${PERPLEX_ExpScanner_SRC})
  set_property(TARGET objlib_expscan_c PROPERTY POSITION_INDEPENDENT_CODE ON)

  add_library(objlib_expparse_c OBJECT ${LEMON_ExpParser_OUTPUTS})
  set_property(TARGET objlib_expparse_c PROPERTY POSITION_INDEPENDENT_CODE ON)

else(SC_GENERATE_LP_SOURCES)
  add_subdirectory(generated)
  include_directories(generated)
endif(SC_GENERATE_LP_SOURCES)

if(MSVC)
  set_property(TARGET objlib_expscan_c APPEND PROPERTY COMPILE_DEFINITIONS "SC_EXPRESS_DLL_EXPORTS")
  set_property(TARGET objlib_expscan_c APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_EXPRESS_DLL_IMPORTS")
  set_property(TARGET objlib_expparse_c APPEND PROPERTY COMPILE_DEFINITIONS "SC_EXPRESS_DLL_EXPORTS")
  set_property(TARGET objlib_expparse_c APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_EXPRESS_DLL_IMPORTS")
endif(MSVC)

set(EXPRESS_SOURCES
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

set(EXPRESS_OBJS)
foreach(_src ${EXPRESS_SOURCES})
  string(REPLACE "." "_" _suffix ${_src})
  set(_objlib "objlib_${_suffix}")
  add_library(${_objlib} OBJECT ${_src})
  add_dependencies(${_objlib} objlib_expscan_c)
  add_dependencies(${_objlib} objlib_expparse_c)
  if(TARGET ExpScanner)
    add_dependencies(${_objlib} ExpScanner)
  endif(TARGET ExpScanner)
  if(MSVC)
    set_property(TARGET ${_objlib} APPEND PROPERTY COMPILE_DEFINITIONS "SC_EXPRESS_DLL_EXPORTS")
    set_property(TARGET ${_objlib} APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_EXPRESS_DLL_IMPORTS")
  endif(MSVC)
  # TODO: probably PIC should be used everywhere...
  set_property(TARGET ${_objlib} PROPERTY POSITION_INDEPENDENT_CODE ON)
  list(APPEND EXPRESS_OBJS $<TARGET_OBJECTS:${_objlib}>)
endforeach()

list(APPEND EXPRESS_OBJS $<TARGET_OBJECTS:objlib_expscan_c>)
list(APPEND EXPRESS_OBJS $<TARGET_OBJECTS:objlib_expparse_c>)


if(SC_GENERATE_LP_SOURCES)
  set_property(TARGET objlib_expparse_c objlib_express_c objlib_lexact_c 
    APPEND PROPERTY INCLUDE_DIRECTORIES "${PERPLEX_ExpScanner_INCLUDE_DIR}")
  set_property(TARGET objlib_expscan_c objlib_express_c objlib_lexact_c
    APPEND PROPERTY INCLUDE_DIRECTORIES "${LEMON_ExpParser_INCLUDE_DIR}")
  # OBJECT libraries are not targets, and so an explicit dependency is required
  set_source_files_properties(express.c lexact.c PROPERTIES OBJECT_DEPENDS "${PERPLEX_ExpScanner_HDR};${LEMON_ExpParser_HDR}")
endif()

if(BUILD_SHARED_LIBS OR NOT BUILD_STATIC_LIBS)
  add_library(express SHARED ${EXPRESS_OBJS})
  if(OPENBSD)
    set_target_properties(express PROPERTIES VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})
  else(OPENBSD)
    set_target_properties(express PROPERTIES VERSION ${SC_VERSION} SOVERSION ${SC_VERSION_MAJOR})
  endif(OPENBSD)
  if(APPLE)
    set_property(TARGET express APPEND PROPERTY LINK_FLAGS "-flat_namespace -undefined suppress")
  endif(APPLE)
  install(TARGETS express
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})

  if(MSVC)
    set_property(TARGET express APPEND PROPERTY COMPILE_DEFINITIONS "SC_EXPRESS_DLL_EXPORTS")
    set_property(TARGET express APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_EXPRESS_DLL_IMPORTS")
  endif(MSVC)
endif()

if(BUILD_STATIC_LIBS)
  add_library(express-static STATIC ${EXPRESS_OBJS})
  install(TARGETS express-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif()

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
if(BUILD_SHARED_LIBS OR NOT BUILD_STATIC_LIBS)
  target_link_libraries(check-express express)
else()
  target_link_libraries(check-express express-static)
endif()
install(TARGETS check-express
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(SC_ENABLE_TESTING)
  add_subdirectory(test)
endif(SC_ENABLE_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
