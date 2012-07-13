include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SCL_SOURCE_DIR}/src/base
)

if(SCL_GENERATE_LP_SOURCES)
   LEMON_TARGET(ExpParser expparse.y expparse.c expparse.h)
   PERPLEX_TARGET(ExpScanner expscan.l ${CMAKE_CURRENT_BINARY_DIR}/expscan.c ${CMAKE_CURRENT_BINARY_DIR}/expscan.h)
   ADD_PERPLEX_LEMON_DEPENDENCY(ExpScanner ExpParser)
   set(EXPRESS_GENERATED_SRCS
      ${LEMON_ExpParser_OUTPUT_SOURCE}
      ${PERPLEX_ExpScanner_OUTPUTS}
      )
else(SCL_GENERATE_LP_SOURCES)
   set(EXPRESS_GENERATED_SRCS
       generated/expparse.c
       generated/expscan.c
       )
   include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated)
   set(BASELINE_INFORMATION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/generated/verification_info.cmake")
   set(PROJECT_CMAKE_DIR "${SCL_SOURCE_DIR}/cmake")
   set(VERIFY_FILELIST 
	   generated/expscan.c 
	   generated/expscan.h 
	   generated/expparse.c 
	   generated/expparse.h
	   )
   configure_file(${SCL_SOURCE_DIR}/cmake/verify_generated.cmake.in
	   ${CMAKE_CURRENT_BINARY_DIR}/express_verify_generated.cmake @ONLY)
   add_custom_command(
	   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/express_verify_generated.sentinel
	   COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/express_verify_generated.cmake
	   COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/express_verify_generated.sentinel
	   DEPENDS ${VERIFY_FILELIST} generated/verification_info.cmake
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
