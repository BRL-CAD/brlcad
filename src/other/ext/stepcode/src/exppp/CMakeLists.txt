add_library(exppp SHARED exppp.c)
target_link_libraries(exppp express)
set_target_properties(exppp PROPERTIES VERSION ${STEPCODE_ABI_VERSION} SOVERSION ${STEPCODE_ABI_SOVERSION})
if(WIN32)
  target_compile_definitions(exppp PRIVATE STEPCODE_EXPPP_DLL_EXPORTS STEPCODE_EXPRESS_BASE_IMPORTS)
endif()
if(APPLE)
  set_target_properties(exppp PROPERTIES LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)

install(TARGETS exppp
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

