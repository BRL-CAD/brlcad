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
  dall.mged
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
  make_bb.mged
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

foreach(testfile ${MGED_TESTS})
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${testfile} TEST_CONTENTS)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/regress_${testfile} "source ${CMAKE_CURRENT_SOURCE_DIR}/regression_resources.tcl\n\n${TEST_CONTENTS}")
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/regress_${testfile})
endforeach(testfile ${MGED_TESTS})

set(G_FILENAME "regress_mged_cmd_functionality.g")
set(MGED_TEST_LIST "")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${G_FILENAME}
  COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_BINARY_DIR}/${G_FILENAME}
  COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mged -c ${CMAKE_CURRENT_BINARY_DIR}/${G_FILENAME} ls
  DEPENDS mged ${MGED_TESTS}
  )

set(test_prev "")
foreach(testfile ${MGED_TESTS})
  string(REGEX REPLACE "([0-9a-z_-]*).mged" "\\1" cmd_root "${testfile}")
  add_custom_target(regress-mged-${cmd_root}
	 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mged -c ${CMAKE_CURRENT_BINARY_DIR}/${G_FILENAME} < ${CMAKE_CURRENT_BINARY_DIR}/regress_${testfile}
    DEPENDS mged ${CMAKE_CURRENT_BINARY_DIR}/${G_FILENAME} ${test_prev}
    )
  set_target_properties(regress-mged-${cmd_root} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  set(MGED_TEST_LIST ${MGED_TEST_LIST} regress-mged-${cmd_root})
  set(test_prev regress-mged-${cmd_root})
endforeach(testfile ${MGED_TESTS})

list(SORT MGED_TEST_LIST)
add_custom_target(regress-mged-cmds DEPENDS ${MGED_TEST_LIST})
set_target_properties(regress-mged-cmds PROPERTIES FOLDER "BRL-CAD Regression Tests")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
