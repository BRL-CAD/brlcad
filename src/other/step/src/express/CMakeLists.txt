YACC_TARGET(ExpParser expparse.y ${CMAKE_CURRENT_BINARY_DIR}/expparse.c)
LEX_TARGET(ExpScanner expscan.l ${CMAKE_CURRENT_BINARY_DIR}/expscan.c)
ADD_LEX_YACC_DEPENDENCY(ExpScanner ExpParser)

set(EXPRESS_SOURCES
    ${YACC_ExpParser_OUTPUTS}
    ${LEX_ExpScanner_OUTPUTS}
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
    yyvars.c
    express.c
)

SET(FEDEX_SOURCES
    fedex.c
)

SET(SYMLINK_SOURCES
    fedex.c
    symlink.c
)

SET(EXPRESS_PRIVATE_HDRS
    conf.h
    exptoks.h
    stack.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_definitions(
    -DHAVE_CONFIG_H
)

SCL_ADDLIB(express "${EXPRESS_SOURCES}" "")

SCL_ADDEXEC(fedex "${FEDEX_SOURCES}" express)

SCL_ADDEXEC(symlink "${SYMLINK_SOURCES}" express)
