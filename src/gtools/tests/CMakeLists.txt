
BRLCAD_ADDEXEC(bigdb bigdb.c "libged;libwdb;librt;libbu" TEST_USESDATA)

BRLCAD_ADD_TEST(NAME slow-bigdb_1gb COMMAND bigdb 1)
BRLCAD_ADD_TEST(NAME slow-bigdb_5gb COMMAND bigdb 5)

set_property(TEST slow-bigdb_1gb PROPERTY SKIP_RETURN_CODE 123)
set_property(TEST slow-bigdb_5gb PROPERTY SKIP_RETURN_CODE 123)

# Test GQA's ability to write out valid plot files, both in single and
# multithreaded modes.

set(GQA_GFILE "${CMAKE_CURRENT_SOURCE_DIR}/../../libged/tests/ged_tests.g")

set(GQA_PFILE "${CMAKE_CURRENT_BINARY_DIR}/ovlp_overlaps.plot3")
BRLCAD_ADD_TEST(NAME gqa_plot COMMAND gqa -P 1 -Ao -p ovlp_ "${GQA_GFILE}" ovlp)
BRLCAD_ADD_TEST(NAME gqa_plot_verify COMMAND bview_plot3 -b "${GQA_PFILE}")
set_property(TEST gqa_plot_verify PROPERTY DEPENDS gqa_plot)
DISTCLEAN("${GQA_PFILE}")

set(GQA_MULTI_PFILE "${CMAKE_CURRENT_BINARY_DIR}/ovlpmulti_overlaps.plot3")
BRLCAD_ADD_TEST(NAME gqa_plot_multithreaded COMMAND gqa -Ao -p ovlpmulti_ "${GQA_GFILE}" ovlp)
BRLCAD_ADD_TEST(NAME gqa_plot_verify_multithreaded COMMAND bview_plot3 -b "${GQA_MULTI_PFILE}")
set_property(TEST gqa_plot_verify_multithreaded PROPERTY DEPENDS gqa_plot_multithreaded)
DISTCLEAN("${GQA_MULTI_PFILE}")


CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

