
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
    ${SC_SOURCE_DIR}/src/base
    ${SC_SOURCE_DIR}/src/cldai
    ${SC_SOURCE_DIR}/src/clstepcore
    ${SC_SOURCE_DIR}/src/clutils
)

SC_ADDLIB(stepeditor "${LIBSTEPEDITOR_SRCS}" "stepcore;stepdai;steputils;base")
