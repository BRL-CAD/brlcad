include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(LEMON)
LEMON_TARGET(ExpParser expparse.y expparse.c expparse.h)
PERPLEX_TARGET(ExpScanner expscan.l ${CMAKE_CURRENT_BINARY_DIR}/expscan.c ${CMAKE_CURRENT_BINARY_DIR}/expscan.h)
ADD_PERPLEX_LEMON_DEPENDENCY(ExpScanner ExpParser)

set(EXPRESS_SOURCES
    ${LEMON_ExpParser_OUTPUT_SOURCE}
    ${PERPLEX_ExpScanner_OUTPUTS}
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

add_definitions(
    -DHAVE_CONFIG_H
)

SCL_ADDLIB(express "${EXPRESS_SOURCES}" "")
SCL_ADDEXEC(fedex "${FEDEX_SOURCES}" express)
