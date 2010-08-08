BISON_TARGET(ExpParser expparse.y ${CMAKE_CURRENT_BINARY_DIR}/expparse.c)
FLEX_TARGET(ExpScanner expscan.l ${CMAKE_CURRENT_BINARY_DIR}/expscan.c COMPILE_FLAGS "-l")
ADD_FLEX_BISON_DEPENDENCY(ExpScanner ExpParser)

set(EXPRESS_SOURCES
    ${BISON_ExpParser_OUTPUTS}
    ${FLEX_ExpScanner_OUTPUTS}
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

add_library(express ${EXPRESS_SOURCES})

add_executable(fedex ${FEDEX_SOURCES})
target_link_libraries(fedex express)

add_executable(symlink ${SYMLINK_SOURCES})
target_link_libraries(symlink express)
