
set(LIBSTEPEDITOR_SRCS
    STEPfile.cc
    STEPfile.inline.cc
    cmdmgr.cc
    SdaiHeaderSchema.cc
    SdaiHeaderSchemaAll.cc
    SdaiHeaderSchemaInit.cc
    SdaiSchemaInit.cc
)

SET(SC_CLEDITOR_HDRS
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

if (NOT SKIP_INSTALL_HEADERS)
	install(FILES ${SC_CLEDITOR_HDRS}
		DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/cleditor)
endif (NOT SKIP_INSTALL_HEADERS)
