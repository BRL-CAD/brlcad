include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SCL_SOURCE_DIR}/src/base
)

find_package(LEMON)
find_package(PERPLEX)
find_package(RE2C)
if(LEMON_EXECUTABLE AND LEMON_TEMPLATE AND PERPLEX_EXECUTABLE AND RE2C_EXECUTABLE)
   include(${SCL_SOURCE_DIR}/cmake/LEMON_Util.cmake)
   include(${SCL_SOURCE_DIR}/cmake/RE2C_Util.cmake)
   include(${SCL_SOURCE_DIR}/cmake/PERPLEX_Util.cmake)
   LEMON_TARGET(ExpParser expparse.y expparse.c expparse.h)
   PERPLEX_TARGET(ExpScanner expscan.l ${CMAKE_CURRENT_BINARY_DIR}/expscan.c ${CMAKE_CURRENT_BINARY_DIR}/expscan.h)
   ADD_PERPLEX_LEMON_DEPENDENCY(ExpScanner ExpParser)
   set(EXPRESS_GENERATED_SRCS
      ${LEMON_ExpParser_OUTPUT_SOURCE}
      ${PERPLEX_ExpScanner_OUTPUTS}
      )
else(LEMON_EXECUTABLE AND LEMON_TEMPLATE AND PERPLEX_EXECUTABLE AND RE2C_EXECUTABLE)
   set(EXPRESS_GENERATED_SRCS
       generated/expparse.c
       generated/expscan.c
       )
   include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated)
endif(LEMON_EXECUTABLE AND LEMON_TEMPLATE AND PERPLEX_EXECUTABLE AND RE2C_EXECUTABLE)

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
