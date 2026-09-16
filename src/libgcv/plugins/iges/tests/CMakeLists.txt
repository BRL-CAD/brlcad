brlcad_addexec(test_iges_document test_iges_document.cpp gcv_iges_core NO_INSTALL)
brlcad_add_test(NAME iges_document_test COMMAND test_iges_document)
brlcad_add_test(NAME iges_document_corpus_test
  COMMAND test_iges_document
    "${CMAKE_CURRENT_SOURCE_DIR}/data/ge6.igs"
    "${CMAKE_CURRENT_SOURCE_DIR}/data/wing128.igs"
    "${CMAKE_CURRENT_SOURCE_DIR}/data/woodie.igs"
)

brlcad_addexec(test_iges_import test_iges_import.cpp gcv_iges_core NO_INSTALL)
brlcad_add_test(NAME iges_import_test COMMAND test_iges_import)

brlcad_addexec(test_iges_writer test_iges_writer.cpp gcv_iges_core NO_INSTALL)
brlcad_add_test(NAME iges_writer_test COMMAND test_iges_writer)

brlcad_addexec(test_iges_orientation test_iges_orientation.cpp gcv_iges_core NO_INSTALL)
brlcad_add_test(NAME iges_orientation_test COMMAND test_iges_orientation)
brlcad_add_test(NAME iges_orientation_io
  COMMAND ${CMAKE_COMMAND}
    -DTEST_ORIENTATION=$<TARGET_FILE:test_iges_orientation>
    -DIGES_G=$<TARGET_FILE:iges-g>
    -DGCV=$<TARGET_FILE:gcv>
    -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/orientation-io
    -P ${CMAKE_CURRENT_SOURCE_DIR}/orientation.cmake)

brlcad_addexec(test_iges_rays test_iges_rays.cpp "librt;libbu" NO_INSTALL)
brlcad_addexec(test_iges_brep test_iges_brep.cpp "librt;libbrep;libbu" NO_INSTALL)

brlcad_addexec(test_gcv_iges test_gcv_iges.cpp "libgcv;gcv_iges_core" NO_INSTALL)
add_dependencies(test_gcv_iges gcv-iges)
brlcad_add_test(NAME gcv_iges_test COMMAND test_gcv_iges "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")

set(iges_test_outputs
  ${CMAKE_CURRENT_BINARY_DIR}/orientation-io
  ${CMAKE_CURRENT_BINARY_DIR}/plugin-native.igs
  ${CMAKE_CURRENT_BINARY_DIR}/plugin-faceted-0.igs
  ${CMAKE_CURRENT_BINARY_DIR}/plugin-faceted-1.igs
  ${CMAKE_CURRENT_BINARY_DIR}/plugin-faceted-2.igs
  ${CMAKE_CURRENT_BINARY_DIR}/plugin-flat.igs
  ${CMAKE_CURRENT_BINARY_DIR}/plugin-brep.igs
  ${CMAKE_CURRENT_BINARY_DIR}/plugin-missing.igs
  ${CMAKE_CURRENT_BINARY_DIR}/plugin-scaled.igs
  )
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${iges_test_outputs}")
distclean(${iges_test_outputs})

cmakefiles(CMakeLists.txt test_iges_document.cpp test_iges_import.cpp test_iges_rays.cpp test_iges_brep.cpp test_iges_writer.cpp
  test_gcv_iges.cpp test_iges_orientation.cpp orientation.cmake data/ge6.igs data/wing128.igs data/woodie.igs
  roundtrip.cmake native-csg.igs mixed-assembly.igs bad-property.igs name-collision.igs mixed-subfigure.igs
  hollerith-boundary.igs numeric-boundary.igs io.cmake datum.igs
  native.cmake native-wedge.igs native-revolve.igs native-extrude.igs test_helpers.cmake export_failure.cmake plugin.cmake)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
