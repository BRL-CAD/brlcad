# Tests for various GED commands

if(TARGET libged)
  brlcad_addexec(regress_mater mater.c libged TEST_USESDATA)
  brlcad_regression_test(regress-ged_mater "regress_mater;ged-mater;density_DATAFILES_cp" EXEC regress_mater)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/regress-ged_mater.log")
  distclean("${CMAKE_CURRENT_BINARY_DIR}/ged_mater_test.g")

  brlcad_addexec(regress_push push.cpp libged TEST_USESDATA)
  brlcad_regression_test(regress-ged_push "regress_push;ged-npush" EXEC regress_push)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/regress-ged_push.log")
  distclean("${CMAKE_CURRENT_BINARY_DIR}/ged_push.g")

  # Phase D regression: coplanar window-frame subtraction pattern.
  # Exercises the instance-aware variant planning pass in the Manifold
  # facetize pipeline (plan.cpp + _booltree_leaf_tess variant resolution).
  brlcad_addexec(regress_facetize regress_facetize.cpp "libged;libwdb;librt;libbu" TEST_USESDATA)
  if(TARGET regress_facetize)
    add_dependencies(regress_facetize ged_plugins)
  endif(TARGET regress_facetize)
  brlcad_regression_test(regress-ged_facetize "regress_facetize;ged-facetize;ged_exec" EXEC regress_facetize)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/regress-ged_facetize.log")

  # lint --raytrace regression: Crofton vs analytic / BoT validation pass.
  # Exercises the raytrace validation pipeline in lint/raytrace.cpp:
  brlcad_addexec(regress_lint_raytrace regress_lint_raytrace.cpp "libged;libwdb;librt;libbu" TEST_USESDATA)
  if(TARGET regress_lint_raytrace)
    add_dependencies(regress_lint_raytrace ged_plugins)
  endif(TARGET regress_lint_raytrace)
  brlcad_regression_test(regress-ged_lint_raytrace "regress_lint_raytrace;ged_exec" EXEC regress_lint_raytrace)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/regress-ged_lint_raytrace.log")
endif(TARGET libged)

cmakefiles(
  CMakeLists.txt
  mater.c
  push.cpp
  regress_facetize.cpp
  regress_lint_raytrace.cpp
  d2push_tests.g
  dpush_tests.g
  dxpush_tests.g
  ppush_tests.g
  push_tests.g
  regress-ged_mater.cmake.in
  regress-ged_push.cmake.in
  regress-ged_facetize.cmake.in
  regress-ged_lint_raytrace.cmake.in
  rpush_tests.g
  xpush_tests.g
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
