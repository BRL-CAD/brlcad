
set(LIBSTEPEDITOR_SRCS
    STEPfile.cc
    STEPfile.inline.cc
    cmdmgr.cc
    SdaiHeaderSchema.cc
    SdaiHeaderSchemaAll.cc
    SdaiHeaderSchemaInit.cc
    SdaiSchemaInit.cc
)

SET(LIBSTEPEDITOR_PRIVATE_HDRS
    STEPfile.h
    cmdmgr.h
    editordefines.h
    SdaiHeaderSchema.h
    SdaiHeaderSchemaClasses.h
    SdaiSchemaInit.h
    seeinfodefault.h
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SCL_SOURCE_DIR}/src/base
    ${SCL_SOURCE_DIR}/src/cldai
    ${SCL_SOURCE_DIR}/src/clstepcore
    ${SCL_SOURCE_DIR}/src/clutils
)

SCL_ADDLIB(stepeditor "${LIBSTEPEDITOR_SRCS}" "stepcore;stepdai;steputils;base")
