set(IGES_PARSER_TEST_SRCS
  ../iges_document.cpp
  test_iges_document.cpp
)

brlcad_addexec(test_iges_document "${IGES_PARSER_TEST_SRCS}" "libbu" NO_INSTALL)
brlcad_add_test(NAME iges_document_test COMMAND test_iges_document)
brlcad_add_test(
  NAME iges_document_corpus_test
  COMMAND test_iges_document
    "${CMAKE_CURRENT_SOURCE_DIR}/../ge6.igs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../wing128.igs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../woodie.igs"
)

set(IGES_IMPORT_TEST_SRCS
  ../iges_document.cpp
  ../iges_brep_import.cpp
  ../iges_import.cpp
  test_iges_import.cpp
)

brlcad_addexec(
  test_iges_import
  "${IGES_IMPORT_TEST_SRCS}"
  "libwdb;librt;libnmg;libbrep;libbn;libbu;${M_LIBRARY}"
  NO_INSTALL
)
brlcad_add_test(NAME iges_import_test COMMAND test_iges_import)

brlcad_addexec(test_iges_rays test_iges_rays.cpp "librt;libbu" NO_INSTALL)
brlcad_addexec(test_iges_brep test_iges_brep.cpp "librt;libbrep;libbu" NO_INSTALL)

brlcad_addexec(test_iges_numbers
  "test_iges_numbers.c;../readnum.c;../readint.c;../readflt.c;../readdbl.c;../readcnv.c;../readname.c;../readstrg.c;../readtime.c"
  "libbu" NO_INSTALL)
brlcad_add_test(NAME iges_numbers_test COMMAND test_iges_numbers)

cmakefiles(CMakeLists.txt test_iges_document.cpp test_iges_import.cpp test_iges_rays.cpp test_iges_brep.cpp
  roundtrip.cmake native-csg.igs mixed-assembly.igs bad-property.igs name-collision.igs mixed-subfigure.igs
  hollerith-boundary.igs numeric-boundary.igs test_iges_numbers.c
  native.cmake native-wedge.igs native-revolve.igs native-extrude.igs test_helpers.cmake export_failure.cmake)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
