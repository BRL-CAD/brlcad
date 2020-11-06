set(UTILS_SRCS
  Str.cc
  dirobj.cc
  errordesc.cc
  gennode.cc
  gennodearray.cc
  gennodelist.cc
  sc_hash.cc
  )

add_library(steputils SHARED ${UTILS_SRCS})
set_target_properties(steputils PROPERTIES VERSION ${STEPCODE_ABI_VERSION} SOVERSION ${STEPCODE_ABI_SOVERSION})
target_compile_definitions(steputils PRIVATE STEPCODE_UTILS_DLL_EXPORTS)
if(WIN32)
  target_link_libraries(steputils shlwapi)
endif()

install(TARGETS steputils
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

