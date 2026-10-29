set(
  MGED_TESTS
  3ptarb.mged
  accept.mged
  adc.mged
  ae.mged
  arb.mged
  arced.mged
  arot.mged
  autoview.mged
  B.mged
  build_region.mged
  center.mged
  clone.mged
  comb.mged
  copymat.mged
  cpi.mged
  cp.mged
  draw_e_erase_d_who.mged
  draw.mged
  edgedir.mged
  erase.mged
  exists.mged
  extrude.mged
  eye_pt.mged
  facedef.mged
  g.mged
  hide.mged
  i.mged
  in.mged
  keypoint.mged
  killall.mged
  kill.mged
  killtree.mged
  knob.mged
  loadview.mged
  lookat.mged
  make.mged
  mirface.mged
  mirror.mged
  mrot.mged
  mvall.mged
  mv.mged
  namespaces.mged
  oed.mged
  orientation.mged
  prim_edit.mged
  orot.mged
  oscale.mged
  overlap_plotting.mged
  overlay.mged
  permute.mged
  plot.mged
  prefix.mged
  ps.mged
  push.mged
  putmat.mged
  qorot.mged
  qvrot.mged
  refresh.mged
  reject.mged
  r.mged
  rm.mged
  rot_edit.mged
  rotobj.mged
  rot_view.mged
  saveview.mged
  sca_edit.mged
  sca_view.mged
  search_exec.mged
  sed.mged
  sed_reset.mged
  set_perspective.mged
  setview.mged
  size.mged
  status.mged
  sv.mged
  tra_edit.mged
  translate.mged
  tra_view.mged
  unhide.mged
  view.mged
  vrot.mged
  who.mged
  xpush.mged
  Z.mged
  zoom.mged
)

if (BRLCAD_ENABLE_TK)
  list(APPEND MGED_TESTS csg_edit.mged)
endif()

set(TSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-mged.cmake.in")

if(TARGET mged)
  foreach(testfile ${MGED_TESTS})
    string(REGEX REPLACE "([0-9a-z_-]*).mged" "\\1" TNAME "${testfile}")

    if(TNAME STREQUAL "")
      message("INTERNAL ERROR: regex replaced to empty test from [$testfile]")
      continue()
    endif(TNAME STREQUAL "")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${testfile}")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${testfile}" TEST_CONTENTS)
    if(TNAME STREQUAL "search_exec")
      string(CONFIGURE "${TEST_CONTENTS}" TEST_CONTENTS @ONLY)
    endif()
    file(
      WRITE
      "${CMAKE_CURRENT_BINARY_DIR}/regress_${testfile}"
      "source {${CMAKE_CURRENT_SOURCE_DIR}/regression_resources.tcl}\n\n${TEST_CONTENTS}"
    )
    distclean("${CMAKE_CURRENT_BINARY_DIR}/regress_${testfile}")

    set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-mged_${TNAME}.log")

    set(TEST_DB "")
    set(TEST_EXPECT "")
    set(TEST_REJECT "FAIL:")
    set(TEST_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/regress_${testfile}")
    set(TEST_KEEP_DB OFF)
    set(TEST_DEPS "mged;rtarea;rt;rtcheck")
    set(TEST_TIMEOUT 300)
    if(TNAME STREQUAL "search_exec")
      set(TEST_DB "${CMAKE_BINARY_DIR}/${DATA_DIR}/db/m35.g")
      set(TEST_EXPECT "PASS search -exec sourced Tcl state")
      list(APPEND TEST_DEPS "m35.g")
    elseif(TNAME STREQUAL "namespaces")
      set(TEST_EXPECT "PASS Tcl command namespaces")
    elseif(TNAME STREQUAL "arb")
      set(TEST_EXPECT "'arb' testing completed")
    elseif(TNAME MATCHES "^(extrude|facedef|mirface|permute)$")
      set(TEST_EXPECT "'${TNAME}' testing completed")
    elseif(TNAME STREQUAL "edgedir")
      set(TEST_EXPECT "'edarb edgedir' testing completed")
    elseif(TNAME STREQUAL "killtree")
      list(APPEND TEST_REJECT "db_lookup(killtree_dup.s) failed")
    elseif(TNAME STREQUAL "oed")
      set(TEST_EXPECT "PASS oed one-path bounding-box keypoint")
    elseif(TNAME STREQUAL "csg_edit")
      set(TEST_EXPECT "PASS: broad MGED CSG edit coverage completed")
      list(APPEND TEST_REJECT "rt_arb_edit:" "rt_edit_knobs_tra:" "Incorrect edit flag")
    elseif(TNAME STREQUAL "prim_edit")
      set(TEST_EXPECT "Primitive parameter editing testing completed")
    elseif(TNAME STREQUAL "sed_reset")
      set(TEST_EXPECT "PASS: 'sed_reset' testing completed")
    elseif(TNAME STREQUAL "overlap_plotting")
      set(TEST_DB "${CMAKE_SOURCE_DIR}/src/libged/tests/ged_tests.g")
      set(TEST_EXPECT "PASS overlap plotting")
      set(TEST_TIMEOUT 20)
    endif()

    if(TNAME STREQUAL "saveview")
      # loadview opens this database to test a saved view of the same geometry.
      set(TEST_KEEP_DB ON)
    endif()

    brlcad_regression_test(
      regress-mged_${TNAME}
      "${TEST_DEPS}"
      TEST_SCRIPT "${TSCRIPT}"
      TIMEOUT "${TEST_TIMEOUT}"
      EXEC mged
    )
    distclean("${CMAKE_CURRENT_BINARY_DIR}/${TNAME}")
    distclean("${LOG_FILE}")
  endforeach(testfile ${MGED_TESTS})

  set_tests_properties(regress-mged_saveview PROPERTIES FIXTURES_SETUP mged_saved_view)
  set_tests_properties(regress-mged_loadview PROPERTIES FIXTURES_REQUIRED mged_saved_view)
  set_tests_properties(regress-mged_plot PROPERTIES FIXTURES_SETUP mged_plot)
  set_tests_properties(regress-mged_overlay PROPERTIES FIXTURES_REQUIRED mged_plot)
  brlcad_add_test(NAME regress-mged_saved_view_cleanup COMMAND ${CMAKE_COMMAND} -E rm -f
    "${CMAKE_CURRENT_BINARY_DIR}/saveview/saveview.g"
    "${CMAKE_CURRENT_BINARY_DIR}/saveview/mged_saveview_test.rt")
  set_tests_properties(regress-mged_saved_view_cleanup PROPERTIES FIXTURES_CLEANUP mged_saved_view)
  brlcad_add_test(NAME regress-mged_plot_cleanup COMMAND ${CMAKE_COMMAND} -E rm -f
    "${CMAKE_CURRENT_BINARY_DIR}/plot/mged_plot_test.plot3")
  set_tests_properties(regress-mged_plot_cleanup PROPERTIES FIXTURES_CLEANUP mged_plot)

  brlcad_regression_test(regress-mged-harness "mged"
    TEST_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-mged-harness.cmake.in"
    EXEC mged)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/harness")

  # Original shell based test
  if(SH_EXEC)
    brlcad_add_test(NAME regress-mged COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/mged.sh" ${CMAKE_SOURCE_DIR})
    brlcad_regression_test(regress-mged "mged;rtarea;rt;rtcheck" TEST_DEFINED)
  endif(SH_EXEC)
endif(TARGET mged)

cmakefiles(
  mged.sh
  mged_test.sh
)

# List of temporary files generated by the mged regression tests.
set(
  mged_outfiles
  mged.g
  mged_gqa_cleared.plot3
  mged_gqa_overlaps.plot3
  mged_overlap_baseline.plot3
  mged.log
  mged_plot_test.plot3
  mged_loadview_test.rt
  mged_overlay_input.plot3
  mged_overlay_test.ps
  mged_rtcheck_overlaps.plot3
  csg_edit_data.bin
  csg_edit_dsp.bin
  mged_saveview_test.rt
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${mged_outfiles}")
distclean(${mged_outfiles})

cmakefiles(
  CMakeLists.txt
  README
  csg_edit.mged
  mged.tcl
  regression_resources.tcl
  search_exec_procs.tcl
  regress-mged.cmake.in
  regress-mged-harness.cmake.in
  ${MGED_TESTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
