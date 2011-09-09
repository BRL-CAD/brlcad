
set(LIBSTEPEDITOR_SRCS
    STEPfile.cc
    STEPfile.inline.cc
    cmdmgr.cc
    dispnode.cc
    dispnodelist.cc
    instmgr.cc
    mgrnode.cc
    mgrnodearray.cc
    mgrnodelist.cc
    needFunc.cc
    s_HEADER_SCHEMA.cc
    s_HEADER_SCHEMA.init.cc
)

SET(LIBSTEPEDITOR_PRIVATE_HDRS
    STEPfile.h
    cmdmgr.h
    dispnode.h
    dispnodelist.h
    editordefines.h
    instmgr.h
    mgrnode.h
    mgrnodearray.h
    mgrnodelist.h
    needFunc.h
    s_HEADER_SCHEMA.h
    seeinfodefault.h
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SCL_SOURCE_DIR}/src/cldai
    ${SCL_SOURCE_DIR}/src/clstepcore
    ${SCL_SOURCE_DIR}/src/clutils
)

add_definitions(
    -DHAVE_CONFIG_H
)

SCL_ADDLIB(stepeditor "${LIBSTEPEDITOR_SRCS}" stepcore)
