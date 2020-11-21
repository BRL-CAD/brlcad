set(MGED_TESTS
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
  erase.mged
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
  oed.mged
  orientation.mged
  orot.mged
  oscale.mged
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
  sed.mged
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

set(TSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-mged.cmake.in")

if (TARGET mged)
  foreach(testfile ${MGED_TESTS})
    string(REGEX REPLACE "([0-9a-z_-]*).mged" "\\1" tname "${testfile}")

    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${testfile}" TEST_CONTENTS)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/regress_${testfile}" "source ${CMAKE_CURRENT_SOURCE_DIR}/regression_resources.tcl\n\n${TEST_CONTENTS}")
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/regress_${testfile}")

    set(TARGET_NAME ${tname})
    set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-mged_${TNAME}.log")

    BRLCAD_REGRESSION_TEST(regress-mged_${tname} "mged;rtarea;rt;rtcheck" TEST_SCRIPT "${TSCRIPT}" EXEC mged)
    DISTCLEAN("${LOG_FILE}")
  endforeach(testfile ${MGED_TESTS})

  # Original shell based test
  if (SH_EXEC)
    BRLCAD_ADD_TEST(NAME regress-mged COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/mged.sh" ${CMAKE_SOURCE_DIR})
    BRLCAD_REGRESSION_TEST(regress-mged "mged;rtarea;rt;rtcheck" TEST_DEFINED)
  endif (SH_EXEC)

endif (TARGET mged)

CMAKEFILES(
  mged.sh
  mged_test.sh
  )

# list of temporary files
set(mged_outfiles
  mged.g
  mged.log
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${mged_outfiles}")
DISTCLEAN(${mged_outfiles})

CMAKEFILES(
  CMakeLists.txt
  mged.tcl
  regression_resources.tcl
  regress-mged.cmake.in
  ${MGED_TESTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
