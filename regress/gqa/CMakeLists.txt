set(GQA_GFILE "${CMAKE_SOURCE_DIR}/src/libged/tests/ged_tests.g")
set(TSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-gqa_plot.cmake.in")
if(TARGET gqa AND TARGET bview_plot3 AND TARGET plot3stat)
  brlcad_regression_test(regress-gqa_plot "bview_plot3;gqa;plot3stat" TEST_SCRIPT "${TSCRIPT}" EXEC gqa VEXEC bview_plot3)
endif(TARGET gqa AND TARGET bview_plot3 AND TARGET plot3stat)

if(SH_EXEC AND TARGET mged AND TARGET gqa AND TARGET nirt)
  brlcad_add_test(NAME regress-gqa COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/gqa.sh" ${CMAKE_SOURCE_DIR})
  brlcad_regression_test(regress-gqa "gqa;mged;nirt" TEST_DEFINED)
endif(SH_EXEC AND TARGET mged AND TARGET gqa AND TARGET nirt)

# list of temporary files
set(
  gqa_outfiles
  density_table.txt
  gqa.adj_air.plot3
  gqa.exp_air.plot3
  gqa.g
  gqa.gaps.plot3
  gqa.log
  gqa.matrix.g
  gqa.matrix.mged
  gqa.modern.options.out
  gqa.modern.invalid_value.out
  gqa.modern.unknown_option.out
  gqa.modern.grid.out
  gqa.modern.grid.json
  gqa.modern.collision.out
  gqa.modern.alias_collision.out
  gqa.alias.json
  gqa.modern.grid_edge.out
  gqa.modern.rotated.out
  gqa.modern.rotated.json
  gqa.modern.grid_defaults.out
  gqa.modern.grid_defaults.json
  gqa.modern.axis_angle.out
  gqa.modern.refine.out
  gqa.modern.refine.json
  gqa.modern.air_excluded.out
  gqa.modern.matrix.out
  gqa.modern.random.out
  gqa.modern.uncertainty.out
  gqa.modern.uncertainty.json
  gqa.modern.centroid.out
  gqa.modern.centroid.json
  gqa.modern.accuracy.out
  gqa.modern.accuracy.json
  gqa.modern.absolute.out
  gqa.modern.absolute.json
  gqa.modern.seed_a.out
  gqa.modern.seed_b.out
  gqa.modern.issues.out
  gqa.modern.issues.json
  gqa.modern.crofton.out
  gqa.modern.crofton.json
  gqa.modern.tolerance.out
  gqa.modern.stability_cap.out
  gqa.modern.defaults.out
  gqa.modern.defaults.json
  gqa.modern.deadline.out
  gqa.modern.deadline.json
  gqa.modern.time_limit.out
  gqa.modern.time_limit.json
  gqa.modern.gaps.out
  gqa.modern.gaps.json
  gqa.modern.air.out
  gqa.modern.air.json
  gqa.mass_air.am.out
  gqa.mass_air.aw.out
  gqa.mass_mix.am.out
  gqa.mass_mix.aw.out
  gqa.mass_mix.u0.out
  gqa.mged
  gqa.overlaps.plot3
  gqa.replay.json
  gqa.replay.out
  gqa.sphere_adaptive.out
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
