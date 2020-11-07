set(EDITOR_SRCS
  STEPfile.cc
  STEPfile.inline.cc
  cmdmgr.cc
  SdaiHeaderSchema.cc
  SdaiHeaderSchemaAll.cc
  SdaiHeaderSchemaInit.cc
  SdaiSchemaInit.cc
  )

add_library(stepeditor SHARED ${EDITOR_SRCS})
set_target_properties(stepeditor PROPERTIES VERSION ${STEPCODE_ABI_VERSION} SOVERSION ${STEPCODE_ABI_SOVERSION})
target_link_libraries(stepeditor stepcore stepdai steputils)
target_compile_definitions(stepeditor PRIVATE
  STEPCODE_EDITOR_DLL_EXPORTS
  STEPCODE_CORE_DLL_IMPORTS
  STEPCODE_DAI_DLL_IMPORTS
  STEPCODE_UTILS_DLL_IMPORTS
  )
install(TARGETS stepeditor
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

