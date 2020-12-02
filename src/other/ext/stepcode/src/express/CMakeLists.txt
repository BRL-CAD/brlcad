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

  add_library(objlib_expscan_c OBJECT ${PERPLEX_ExpScanner_SRC})
  set_property(TARGET objlib_expscan_c PROPERTY POSITION_INDEPENDENT_CODE ON)

  add_library(objlib_expparse_c OBJECT ${LEMON_ExpParser_SRC})
  set_property(TARGET objlib_expparse_c PROPERTY POSITION_INDEPENDENT_CODE ON)

else(SC_GENERATE_LP_SOURCES)
  add_subdirectory(generated)
  include_directories(generated)
endif(SC_GENERATE_LP_SOURCES)

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
  set_source_files_properties(express.c lexact.c parse_data.h PROPERTIES OBJECT_DEPENDS "${PERPLEX_ExpScanner_HDR};${LEMON_ExpParser_HDR}")
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

SET(EXPRESS_PRIVATE_HDRS
  exptoks.h
  stack.h
  )

variable_watch(SC_ADDLIB_EXPRESS_ARG_LINK_LIBRARIES)
variable_watch(SC_ADDLIB_EXPRESS-STATIC_ARG_LINK_LIBRARIES)

if($CACHE{SC_BUILD_SHARED_LIBS})
  SC_ADDLIB(express SHARED SOURCES "dummy.c" ${EXPRESS_OBJS} LINK_LIBRARIES base)
  if(WIN32)
    target_compile_definitions(express PRIVATE SC_EXPRESS_DLL_EXPORTS)
  endif()

  if(NOT SC_IS_SUBBUILD AND SC_GIT_VERSION)
    add_dependencies(express version_string)
  endif()
endif()

if($CACHE{SC_BUILD_STATIC_LIBS})
  SC_ADDLIB(express-static STATIC SOURCES "dummy.c" ${EXPRESS_OBJS} LINK_LIBRARIES base-static)
 
  if(NOT SC_IS_SUBBUILD AND SC_GIT_VERSION)
    add_dependencies(express-static version_string)
  endif()
endif()

SC_ADDEXEC(check-express SOURCES ${CHECK_EXPRESS_SOURCES} LINK_LIBRARIES express base ${SC_EXEC_NOINSTALL})

if(SC_ENABLE_TESTING)
  add_subdirectory(test)
endif(SC_ENABLE_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
