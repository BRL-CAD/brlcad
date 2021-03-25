
set(LIBSTEPCORE_SRCS
  aggrTypeDescriptor.cc
  attrDescriptor.cc
  attrDescriptorList.cc
  collect.cc
  complexlist.cc
  create_Aggr.cc
  derivedAttribute.cc
  dictSchema.cc
  dispnode.cc
  dispnodelist.cc
  entityDescriptor.cc
  entityDescriptorList.cc
  entlist.cc
  entnode.cc
  enumTypeDescriptor.cc
  explicitItemId.cc
  globalRule.cc
  implicitItemId.cc
  instmgr.cc
  interfaceSpec.cc
  interfacedItem.cc
  inverseAttribute.cc
  inverseAttributeList.cc
  match-ors.cc
  mgrnode.cc
  mgrnodearray.cc
  mgrnodelist.cc
  multlist.cc
  needFunc.cc
  non-ors.cc
  orlist.cc
  print.cc
  read_func.cc
  Registry.cc
  schRename.cc
  sdai.cc
  sdaiApplication_instance.cc
  sdaiSelect.cc
  selectTypeDescriptor.cc
  SingleLinkList.cc
  STEPaggregate.cc
  STEPaggrBinary.cc
  STEPaggrEntity.cc
  STEPaggrEnum.cc
  STEPaggrGeneric.cc
  STEPaggrInt.cc
  STEPaggrReal.cc
  STEPaggrSelect.cc
  STEPaggrString.cc
  STEPattribute.cc
  STEPattributeList.cc
  STEPcomplex.cc
  STEPinvAttrList.cc
  STEPundefined.cc
  trynext.cc
  typeDescriptor.cc
  typeDescriptorList.cc
  typeOrRuleVar.cc
  uniquenessRule.cc
  whereRule.cc
)

set(SC_CLSTEPCORE_HDRS
  aggrTypeDescriptor.h
  attrDescriptor.h
  attrDescriptorList.h
  baseType.h
  complexSupport.h
  create_Aggr.h
  derivedAttribute.h
  dictSchema.h
  dictdefs.h
  dictionaryInstance.h
  dispnode.h
  dispnodelist.h
  entityDescriptor.h
  entityDescriptorList.h
  enumTypeDescriptor.h
  ExpDict.h
  explicitItemId.h
  globalRule.h
  implicitItemId.h
  instmgr.h
  interfaceSpec.h
  interfacedItem.h
  inverseAttribute.h
  inverseAttributeList.h
  mgrnode.h
  mgrnodearray.h
  mgrnodelist.h
  needFunc.h
  read_func.h
  realTypeDescriptor.h
  Registry.h
  schRename.h
  sdai.h
  sdaiApplication_instance.h
  sdaiSelect.h
  selectTypeDescriptor.h
  SingleLinkList.h
  STEPaggregate.h
  STEPaggrBinary.h
  STEPaggrEntity.h
  STEPaggrEnum.h
  STEPaggrGeneric.h
  STEPaggrInt.h
  STEPaggrReal.h
  STEPaggrSelect.h
  STEPaggrString.h
  STEPattribute.h
  STEPattributeList.h
  STEPcomplex.h
  STEPinvAttrList.h
  STEPundefined.h
  stringTypeDescriptor.h
  SubSuperIterators.h
  typeDescriptor.h
  typeDescriptorList.h
  typeOrRuleVar.h
  uniquenessRule.h
  whereRule.h
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SC_SOURCE_DIR}/src/base
  ${SC_SOURCE_DIR}/src/cldai
  ${SC_SOURCE_DIR}/src/cleditor
  ${SC_SOURCE_DIR}/src/clutils
  )

set(_libdeps steputils stepdai base)

if(BUILD_SHARED_LIBS)
  SC_ADDLIB(stepcore SHARED SOURCES ${LIBSTEPCORE_SRCS} LINK_LIBRARIES ${_libdeps})
  if(WIN32)
    target_compile_definitions(stepcore PRIVATE SC_CORE_DLL_EXPORTS)
  endif()
endif()

if(BUILD_STATIC_LIBS)
  SC_ADDLIB(stepcore-static STATIC SOURCES ${LIBSTEPCORE_SRCS} LINK_LIBRARIES $<JOIN:${_libdeps},-static>-static)
endif()

install(FILES ${SC_CLSTEPCORE_HDRS}
  DESTINATION ${INCLUDE_DIR}/stepcode/clstepcore)

if(SC_ENABLE_TESTING)
  add_subdirectory(test)
endif(SC_ENABLE_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

