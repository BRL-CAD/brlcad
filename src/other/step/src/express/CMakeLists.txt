include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SCL_SOURCE_DIR}/src/base
)

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
string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}" "generated" CACHED_FILELIST "${BUILD_OUTPUT_FILELIST}")

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
	   DEPENDS ${INPUT_FILELIST} ${TEMPLATE_FILELIST} ${CACHED_FILELIST} generated/verification_info.cmake ${PROJECT_CMAKE_DIR}/Generated_Source_Utils.cmake
	   )
   add_custom_target(express_verify DEPENDS ${PERPLEX_ExpScanner_OUTPUTS} ${LEMON_ExpParser_OUTPUT_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/express_manage_generated.sentinel)

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
    inithook.c
    express.c
)

SET(FEDEX_SOURCES
    fedex.c
)

SET(EXPRESS_PRIVATE_HDRS
    exptoks.h
    stack.h
)

SCL_ADDLIB(express "${EXPRESS_SOURCES}" "base")
SCL_ADDEXEC(fedex "${FEDEX_SOURCES}" express)

add_dependencies(fedex version_string)
