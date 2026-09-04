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

cmakefiles(CMakeLists.txt test_iges_document.cpp test_iges_import.cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
