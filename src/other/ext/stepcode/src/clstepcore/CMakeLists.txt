set(STEPCORE_SRCS
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

set(STEPCORE_HDRS
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

add_library(stepcore-obj OBJECT ${STEPCORE_SRCS})
set_property(TARGET stepcore-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
add_dependencies(stepcore-obj base)
if(MSVC)
  set_property(TARGET stepcore-obj APPEND PROPERTY COMPILE_DEFINITIONS "SC_CORE_DLL_EXPORTS")
endif(MSVC)

add_library(stepcore SHARED $<TARGET_OBJECTS:stepcore-obj>)
target_link_libraries(stepcore steputils stepdai base)
if(OPENBSD)
  set_target_properties(stepcore PROPERTIES VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})
else(OPENBSD)
  set_target_properties(stepcore PROPERTIES VERSION ${SC_VERSION} SOVERSION ${SC_VERSION_MAJOR})
endif(OPENBSD)
if(APPLE)
  set_property(TARGET stepcore APPEND PROPERTY LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
if(MSVC)
  set_property(TARGET stepcore APPEND PROPERTY COMPILE_DEFINITIONS "SC_CORE_DLL_EXPORTS")
  set_property(TARGET stepcore APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_CORE_DLL_IMPORTS")
endif(MSVC)
install(TARGETS stepcore
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (BUILD_STATIC_LIBS)
  add_library(stepcore-static STATIC $<TARGET_OBJECTS:stepcore-obj>)
  install(TARGETS stepcore-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

install(FILES ${STEPCORE_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/clstepcore)

if(SC_ENABLE_TESTING)
  add_subdirectory(test)
endif(SC_ENABLE_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

