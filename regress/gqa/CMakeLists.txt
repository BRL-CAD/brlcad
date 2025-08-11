set(GQA_GFILE "${CMAKE_SOURCE_DIR}/src/libged/tests/ged_tests.g")
set(TSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-gqa_plot.cmake.in")
if(TARGET gqa AND TARGET bview_plot3)
  brlcad_regression_test(regress-gqa_plot "bview_plot3;gqa" TEST_SCRIPT "${TSCRIPT}" EXEC gqa VEXEC bview_plot3)
endif(TARGET gqa AND TARGET bview_plot3)

if(SH_EXEC AND TARGET mged AND TARGET gqa)
  brlcad_add_test(NAME regress-gqa COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/gqa.sh" ${CMAKE_SOURCE_DIR})
  brlcad_regression_test(regress-gqa "gqa;mged" TEST_DEFINED)
endif(SH_EXEC AND TARGET mged AND TARGET gqa)

# list of temporary files
set(
  gqa_outfiles
  density_table.txt
  gqa.adj_air.plot3
  gqa.exp_air.plot3
  gqa.g
  gqa.gaps.plot3
  gqa.log
  gqa.mged
  gqa.overlaps.plot3
  gqa.volume.plot3
  ovlp_overlaps.plot3
  ovlpmulti_overlaps.plot3
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${gqa_outfiles}")
distclean(${gqa_outfiles})

cmakefiles(
  CMakeLists.txt
  gqa.sh
  regress-gqa_plot.cmake.in
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
