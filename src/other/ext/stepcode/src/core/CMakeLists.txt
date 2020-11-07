set(CORE_SRCS
  ExpDict.cc
  ExpDict.inline.cc
  Registry.inline.cc
  STEPaggregate.cc
  STEPattribute.cc
  STEPattribute.inline.cc
  STEPattributeList.cc
  STEPcomplex.cc
  STEPundefined.cc
  SingleLinkList.cc
  SingleLinkList.inline.cc
  collect.cc
  complexlist.cc
  dispnode.cc
  dispnodelist.cc
  entlist.cc
  entnode.cc
  instmgr.cc
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
  sdai.cc
  sdaiApplication_instance.cc
  sdaiSelect.cc
  trynext.cc
  )

add_library(stepcore SHARED ${CORE_SRCS})
set_target_properties(stepcore PROPERTIES VERSION ${STEPCODE_ABI_VERSION} SOVERSION ${STEPCODE_ABI_SOVERSION})
target_link_libraries(stepcore steputils stepdai)
target_compile_definitions(stepcore PRIVATE
  STEPCODE_CORE_DLL_EXPORTS
  STEPCODE_UTILS_DLL_IMPORTS
  STEPCODE_DAI_DLL_IMPORTS
  )
install(TARGETS stepcore
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

