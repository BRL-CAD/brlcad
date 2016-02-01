include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SC_SOURCE_DIR}/src/base
  )

# Set up the information we need to feed the generated source management
# scripts
set(BASELINE_INFORMATION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/generated/verification_info.cmake")
set(PROJECT_CMAKE_DIR "${SC_SOURCE_DIR}/cmake")
set(MD5_FILELIST
  "${CMAKE_CURRENT_SOURCE_DIR}/expscan.l"
  "${CMAKE_CURRENT_SOURCE_DIR}/expparse.y"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/expscan.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/expscan.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/expparse.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/expparse.h"
  )
configure_file(${SC_SOURCE_DIR}/cmake/md5_gen.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/express_md5gen.cmake @ONLY)
configure_file(${SC_SOURCE_DIR}/cmake/md5_verify.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/express_verify.cmake @ONLY)

# Convenience target to generate an updated verification_info.cmake file
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/express_md5gen.sentinel
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/express_md5gen.cmake
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/express_md5gen.sentinel
  )
add_custom_target(express_md5gen DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/express_md5gen.sentinel)

# Target for actually checking cached MD5 sums against files
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/express_verify.sentinel
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/express_verify.cmake
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/express_verify.sentinel
  DEPENDS ${MD5_FILELIST}
  )
add_custom_target(express_verify DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/express_verify.sentinel)


# Depending on whether we're using pre-generated sources or building them on
# the fly, set up targets and source lists.
if(SC_GENERATE_LP_SOURCES)
  LEMON_TARGET(ExpParser expparse.y)
  PERPLEX_TARGET(ExpScanner expscan.l)
  ADD_PERPLEX_LEMON_DEPENDENCY(ExpScanner ExpParser)
  set(SCL_SO_SRCS
    ${LEMON_ExpParser_SRC}
    ${PERPLEX_ExpScanner_SRC}
    )

  LEMON_TARGET(ExpParser_static expparse.y)
  PERPLEX_TARGET(ExpScanner_static expscan.l)
  ADD_PERPLEX_LEMON_DEPENDENCY(ExpScanner_static ExpParser_static)
  set(SCL_STATIC_SRCS
    ${LEMON_ExpParser_static_SRC}
    ${PERPLEX_ExpScanner_static_SRC}
    )
else(SC_GENERATE_LP_SOURCES)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated)
  set(EXPRESS_GENERATED_SRCS
    generated/expparse.c
    generated/expscan.c
    )
  set(SCL_SO_SRCS ${EXPRESS_GENERATED_SRCS})
  set(SCL_STATIC_SRCS ${EXPRESS_GENERATED_SRCS})
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
  lexact.c
  linklist.c
  error.c
  dict.c
  hash.c
  memory.c
  object.c
  express.c
  ordered_attrs.cc
  )

# TODO
# Currently, fedex.c provides the main() for multiple programs. These programs
# provide custom behavior by defining EXPRESSinit_init (called by fedex.c's
# main) which sets other callbacks.
#
# What would be better is to remove the call to EXPRESSinit_init and turn
# fedex.c's main() into an express library function. That way, programs only
# need to link against express (not fedex.o too) and they can do initialization
# in their own main (instead of in a local EXPRESSinit_init definition).
SET(CHECK_EXPRESS_SOURCES
  fedex.c
  inithook.c
  )

SET(EXPRESS_PRIVATE_HDRS
  exptoks.h
  stack.h
  )

SC_ADDLIB(express "${EXPRESS_SOURCES}" "base" SO_SRCS "${SCL_SO_SRCS}" STATIC_SRCS "${SCL_STATIC_SRCS}")
if(SC_GENERATE_LP_SOURCES)
  set_property(TARGET express APPEND PROPERTY INCLUDE_DIRECTORIES "${PERPLEX_ExpScanner_INCLUDE_DIR}")
  set_property(TARGET express APPEND PROPERTY INCLUDE_DIRECTORIES "${LEMON_ExpParser_INCLUDE_DIR}")
  if (TARGET express-static)
    set_property(TARGET express-static APPEND PROPERTY INCLUDE_DIRECTORIES "${PERPLEX_ExpScanner_static_INCLUDE_DIR}")
    set_property(TARGET express-static APPEND PROPERTY INCLUDE_DIRECTORIES "${LEMON_ExpParser_static_INCLUDE_DIR}")
  endif (TARGET express-static)
endif(SC_GENERATE_LP_SOURCES)

if(SC_GENERATE_LP_SOURCES)
  add_custom_command(TARGET express POST_BUILD
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/express_verify.cmake
    )
else(SC_GENERATE_LP_SOURCES)
  add_dependencies(express express_verify)
endif(SC_GENERATE_LP_SOURCES)
if(NOT SC_IS_SUBBUILD)
  add_dependencies(express version_string)
endif(NOT SC_IS_SUBBUILD)
SC_ADDEXEC("check-express" "${CHECK_EXPRESS_SOURCES}" "express;base"  ${SC_EXEC_NOINSTALL})

#add_subdirectory(test)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
