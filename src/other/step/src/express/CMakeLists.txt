include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SCL_SOURCE_DIR}/src/base
  )

# Set up the information we need to feed the generated source management
# scripts
set(BASELINE_INFORMATION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/generated/verification_info.cmake")
set(PROJECT_CMAKE_DIR "${SCL_SOURCE_DIR}/cmake")
set(INPUT_FILELIST expscan.l expparse.y)
set(TEMPLATE_FILELIST ${LEMON_TEMPLATE} ${PERPLEX_TEMPLATE})
set(BUILD_OUTPUT_FILELIST
  ${CMAKE_CURRENT_BINARY_DIR}/expscan.c
  ${CMAKE_CURRENT_BINARY_DIR}/expscan.h
  ${CMAKE_CURRENT_BINARY_DIR}/expparse.c
  ${CMAKE_CURRENT_BINARY_DIR}/expparse.h
  )
set(CACHED_FILES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}" "${CACHED_FILES_DIR}" CACHED_FILELIST "${BUILD_OUTPUT_FILELIST}")
set(SYNC_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/express_sync_generated.cmake")
set(SYNC_TARGET_NAME "express_sync_generated_files")

# Depending on whether we're using pre-generated sources or building them on
# the fly, set up targets and source lists.
if(SCL_GENERATE_LP_SOURCES)
  LEMON_TARGET(ExpParser expparse.y expparse.c expparse.h)
  PERPLEX_TARGET(ExpScanner expscan.l ${CMAKE_CURRENT_BINARY_DIR}/expscan.c ${CMAKE_CURRENT_BINARY_DIR}/expscan.h)
  ADD_PERPLEX_LEMON_DEPENDENCY(ExpScanner ExpParser)
  set(EXPRESS_GENERATED_SRCS
    ${LEMON_ExpParser_OUTPUT_SOURCE}
    ${PERPLEX_ExpScanner_OUTPUTS}
    )
  configure_file(${SCL_SOURCE_DIR}/cmake/manage_generated.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/express_manage_generated.cmake @ONLY)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/express_manage_generated.sentinel
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/express_manage_generated.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/express_manage_generated.sentinel
    DEPENDS ${INPUT_FILELIST} ${TEMPLATE_FILELIST} ${CACHED_FILELIST} ${PERPLEX_ExpScanner_OUTPUTS} ${LEMON_ExpParser_OUTPUT_SOURCE} generated/verification_info.cmake ${PROJECT_CMAKE_DIR}/Generated_Source_Utils.cmake
    )
  add_custom_target(express_verify DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/express_manage_generated.sentinel)

  configure_file(${SCL_SOURCE_DIR}/cmake/sync_generated.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/express_sync_generated.cmake @ONLY)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/express_sync_generated.sentinel
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/express_sync_generated.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/express_sync_generated.sentinel
    DEPENDS ${INPUT_FILELIST} ${TEMPLATE_FILELIST} ${PERPLEX_ExpScanner_OUTPUTS} ${LEMON_ExpParser_OUTPUT_SOURCE} ${PROJECT_CMAKE_DIR}/Generated_Source_Utils.cmake
    )
  add_custom_target(express_sync_generated_files DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/express_sync_generated.sentinel)

else(SCL_GENERATE_LP_SOURCES)

  set(EXPRESS_GENERATED_SRCS
    generated/expparse.c
    generated/expscan.c
    )
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated)
  configure_file(${SCL_SOURCE_DIR}/cmake/verify_generated.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/express_verify_generated.cmake @ONLY)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/express_verify_generated.sentinel
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/express_verify_generated.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/express_verify_generated.sentinel
    DEPENDS ${INPUT_FILELIST} ${CACHED_FILELIST} generated/verification_info.cmake ${PROJECT_CMAKE_DIR}/Generated_Source_Utils.cmake
    )
  add_custom_target(express_verify DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/express_verify_generated.sentinel)

endif(SCL_GENERATE_LP_SOURCES)

set(EXPRESS_SOURCES
  ${EXPRESS_GENERATED_SRCS}
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

SCL_ADDLIB(express "${EXPRESS_SOURCES}" "base")
add_dependencies(express express_verify)
add_dependencies(express version_string)
SCL_ADDEXEC("check-express" "${CHECK_EXPRESS_SOURCES}" "express base" )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
