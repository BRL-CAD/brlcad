
set(LIBSTEPCORE_SRCS
    sdaiApplication_instance.cc
    STEPcomplex.cc
    STEPattribute.cc
    STEPattribute.inline.cc
    sdai.cc
    sdaiEnum.cc
    sdaiString.cc
    sdaiSelect.cc
    sdaiBinary.cc
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
)

SET(LIBSTEPCORE_PRIVATE_HDRS
    baseType.h
    complexSupport.h
    dictdefs.h
    ExpDict.h
    read_func.h
    Registry.h
    scl_osschema.h
    sdaiApplication_instance.h
    sdaiBinary.h
    sdaiEnum.h
    sdai.h
    sdaiSelect.h
    sdaiString.h
    Select.h
    SingleLinkList.h
    STEPaggregate.h
    STEPattribute.h
    STEPattributeList.h
    STEPcomplex.h
    STEPundefined.h
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SCL_SOURCE_DIR}/src/cldai
    ${SCL_SOURCE_DIR}/src/cleditor
    ${SCL_SOURCE_DIR}/src/clutils
)

add_definitions(
    -DHAVE_CONFIG_H
)

SCL_ADDLIB(stepcore "${LIBSTEPCORE_SRCS}" "express steputils")
if(APPLE)
   set_target_properties(stepcore PROPERTIES LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
