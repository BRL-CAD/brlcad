BRLCAD_ADDEXEC(ged_test_tops test_tops.c libged TEST)
BRLCAD_ADD_TEST(NAME ged_test_tops_moss COMMAND ged_test_tops "${CMAKE_CURRENT_SOURCE_DIR}/ged_tests.g")

BRLCAD_ADDEXEC(ged_test_list test_list.c libged TEST)
BRLCAD_ADD_TEST(NAME ged_test_list COMMAND ged_test_list 2)

BRLCAD_ADDEXEC(ged_test_plugins plugins.cpp libged TEST)


# Test GQA's ability to write out valid plot files, both in single and
# multithreaded modes.

set(GQA_PFILE "${CMAKE_CURRENT_BINARY_DIR}/ovlp_overlaps.plot3")
BRLCAD_ADD_TEST(NAME ged_test_gqa_plot COMMAND gqa -P 1 -Ao -p ovlp_ "${CMAKE_CURRENT_SOURCE_DIR}/ged_tests.g" ovlp)
BRLCAD_ADD_TEST(NAME ged_test_gqa_plot_verify COMMAND bn_plot3 -b "${GQA_PFILE}")
set_property(TEST ged_test_gqa_plot_verify PROPERTY DEPENDS ged_test_gqa_plot)
DISTCLEAN("${GQA_PFILE}")

set(GQA_MULTI_PFILE "${CMAKE_CURRENT_BINARY_DIR}/ovlpmulti_overlaps.plot3")
BRLCAD_ADD_TEST(NAME ged_test_gqa_plot_multithreaded COMMAND gqa -Ao -p ovlpmulti_ "${CMAKE_CURRENT_SOURCE_DIR}/ged_tests.g" ovlp)
BRLCAD_ADD_TEST(NAME ged_test_gqa_plot_verify_multithreaded COMMAND bn_plot3 -b "${GQA_MULTI_PFILE}")
set_property(TEST ged_test_gqa_plot_verify_multithreaded PROPERTY DEPENDS ged_test_gqa_plot_multithreaded)
DISTCLEAN("${GQA_MULTI_PFILE}")

# Note: it is particularly important that the lint tests in particular are kept
# in a separate file, since its bad input examples stand an excellent chance of
# breaking commands.
CMAKEFILES(
  lint_tests.g
  obb_tests.g
  ged_tests.g
  pnts_in_out.g
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

