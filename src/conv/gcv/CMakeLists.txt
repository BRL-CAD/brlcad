brlcad_addexec(gcv gcv.c "libgcv;libbn;libbu;${M_LIBRARY}" FOLDER Conv)
add_dependencies(gcv gcv_plugins)

if(TARGET gcv AND TARGET test_gcv_mime)
  add_dependencies(test_gcv_mime gcv)
  brlcad_add_test(NAME gcv_mime_cli COMMAND ${CMAKE_COMMAND}
    "-DSOURCE_DIR=${BRLCAD_SOURCE_DIR}" "-DGCV=$<TARGET_FILE:gcv>" "-DHAS_ASSETIMPORT=$<TARGET_EXISTS:gcv-assetimport>" "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/mime_cli"
    -P "${BRLCAD_SOURCE_DIR}/src/libgcv/tests/mime_cli.cmake")
  distclean("${CMAKE_CURRENT_BINARY_DIR}/mime_cli")
  if(TARGET gcv-assetimport)
    brlcad_add_test(NAME gcv_mime_sdk_writers COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${BRLCAD_SOURCE_DIR}" "-DGCV=$<TARGET_FILE:gcv>" "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/sdk_writers"
      -P "${BRLCAD_SOURCE_DIR}/src/libgcv/tests/sdk_writers.cmake")
    distclean("${CMAKE_CURRENT_BINARY_DIR}/sdk_writers")
  endif()
endif()

cmakefiles(gcv.sh)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
