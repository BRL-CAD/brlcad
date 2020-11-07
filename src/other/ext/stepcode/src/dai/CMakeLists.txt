set(DAI_SRCS
  sdaiApplication_instance_set.cc
  sdaiBinary.cc
  sdaiDaObject.cc
  sdaiEntity_extent.cc
  sdaiEntity_extent_set.cc
  sdaiEnum.cc
  sdaiModel_contents.cc
  sdaiModel_contents_list.cc
  sdaiObject.cc
  sdaiSession_instance.cc
  sdaiString.cc
  )

add_library(stepdai SHARED ${DAI_SRCS})
set_target_properties(stepdai PROPERTIES VERSION ${STEPCODE_ABI_VERSION} SOVERSION ${STEPCODE_ABI_SOVERSION})
target_link_libraries(stepdai steputils)
target_compile_definitions(stepdai PRIVATE
  STEPCODE_DAI_DLL_EXPORTS
  STEPCODE_UTILS_DLL_IMPORTS
  )
install(TARGETS stepdai
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

