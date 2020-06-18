
set(LIBSTEPCORE_SRCS
    sdaiApplication_instance.cc
    STEPcomplex.cc
    STEPattribute.cc
    STEPattribute.inline.cc
    sdai.cc
    sdaiSelect.cc
    STEPaggregate.cc
    STEPundefined.cc
    STEPattributeList.cc
    SingleLinkList.cc
    SingleLinkList.inline.cc
    Registry.inline.cc
    ExpDict.cc
    ExpDict.inline.cc
    read_func.cc
    collect.cc
    complexlist.cc
    entlist.cc
    multlist.cc
    orlist.cc
    entnode.cc
    non-ors.cc
    match-ors.cc
    trynext.cc
    print.cc
    instmgr.cc
    mgrnode.cc
    mgrnodearray.cc
    mgrnodelist.cc
    dispnode.cc
    dispnodelist.cc
    needFunc.cc
)

SET(SC_CLSTEPCORE_HDRS
    baseType.h
    complexSupport.h
    dictdefs.h
    ExpDict.h
    read_func.h
    Registry.h
    sdaiApplication_instance.h
    sdai.h
    sdaiSelect.h
    SingleLinkList.h
    STEPaggregate.h
    STEPattribute.h
    STEPattributeList.h
    STEPcomplex.h
    STEPundefined.h
    instmgr.h
    mgrnode.h
    mgrnodearray.h
    mgrnodelist.h
    dispnode.h
    dispnodelist.h
    needFunc.h
    SubSuperIterators.h
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SC_SOURCE_DIR}/src/base
    ${SC_SOURCE_DIR}/src/cldai
    ${SC_SOURCE_DIR}/src/cleditor
    ${SC_SOURCE_DIR}/src/clutils
)

SC_ADDLIB(stepcore "${LIBSTEPCORE_SRCS}" "steputils;stepdai;base")

if (NOT SKIP_INSTALL_HEADERS)
	install(FILES ${SC_CLSTEPCORE_HDRS}
		DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/clstepcore)
endif (NOT SKIP_INSTALL_HEADERS)
