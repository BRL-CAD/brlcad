YACC_TARGET(ExpParser expparse.y ${CMAKE_CURRENT_BINARY_DIR}/expparse.c)
LEX_TARGET(ExpScanner expscan.l ${CMAKE_CURRENT_BINARY_DIR}/expscan.c COMPILE_FLAGS "-l")
ADD_LEX_YACC_DEPENDENCY(ExpScanner ExpParser)

LEMON_TARGET(NewExpParser expparse_new.y ${CMAKE_CURRENT_BINARY_DIR}/expparse_new.c)
LEX_TARGET(NewExpScanner expscan.re ${CMAKE_CURRENT_BINARY_DIR}/expscan_new.c COMPILE_FLAGS "-l")
SET_SOURCE_FILES_PROPERTIES(${LEX_NewExpScanner_OUTPUTS} PROPERTIES OBJECT_DEPENDS ${LEMON_NewExpParser_OUTPUT_HEADER})

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

set(NEW_EXPRESS_SOURCES
    ${LEMON_NewExpParser_OUTPUTS}
    ${LEX_NewExpScanner_OUTPUTS}
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
    express_new.c
)

SET(FEDEX_SOURCES
    fedex.c
)

SET(NEW_FEDEX_SOURCES
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

SCL_ADDLIB(newexpress "${NEW_EXPRESS_SOURCES}" "")
SCL_ADDEXEC(fedex_new "${NEW_FEDEX_SOURCES}" newexpress)
