set(test_deps
  dm-swrast
  ged-autoview
  ged-close
  ged-draw
  ged-dm
  ged-facetize
  ged-open
  ged-png2fb
  ged-view
  ged-zap
  )


set(basic_view_test_ctrls
  v001_ctrl.png
  v002_ctrl.png
  v003_ctrl.png
  v004_ctrl.png
  v005_ctrl.png
  v006_ctrl.png
  v007_ctrl.png
  v008_ctrl.png
  v009_ctrl.png
  v010_ctrl.png
  v011_ctrl.png
  v012_ctrl.png
  v013_ctrl.png
  v014_ctrl.png
  v015_ctrl.png
  v016_ctrl.png
  v017_ctrl.png
  v018_ctrl.png
  v019_ctrl.png
  v020_ctrl.png
  v021_ctrl.png
  v022_ctrl.png
  v023_ctrl.png
  v024_ctrl.png
  v025_ctrl.png
  v026_ctrl.png
  )
BRLCAD_ADDEXEC(ged_test_draw draw.cpp "libged;libdm" TEST)
add_dependencies(ged_test_draw ${test_deps})
BRLCAD_ADD_TEST(NAME ged_test_drawing_basic COMMAND ged_test_draw "${CMAKE_CURRENT_SOURCE_DIR}")


set(faceplate_test_ctrls
  fp001_ctrl.png
  fp002_ctrl.png
  fp003_ctrl.png
  fp004_ctrl.png
  fp005_ctrl.png
  fp006_ctrl.png
  fp007_ctrl.png
  fp008_ctrl.png
  fp009_ctrl.png
  )
BRLCAD_ADDEXEC(ged_test_faceplate faceplate.cpp "libged;libdm" TEST)
add_dependencies(ged_test_faceplate ${test_deps})
BRLCAD_ADD_TEST(NAME ged_test_drawing_faceplate COMMAND ged_test_faceplate "${CMAKE_CURRENT_SOURCE_DIR}")


set(lod_test_ctrls
  lod001_ctrl.png
  lod002_ctrl.png
  lod003_ctrl.png
  lod004_ctrl.png
  lod005_ctrl.png
  lod006_ctrl.png
  )
BRLCAD_ADDEXEC(ged_test_lod lod.cpp "libged;libdm" TEST)
add_dependencies(ged_test_lod ${test_deps})
BRLCAD_ADD_TEST(NAME ged_test_drawing_lod COMMAND ged_test_lod "${CMAKE_CURRENT_SOURCE_DIR}")



CMAKEFILES(
  README
  empty.png
  moss.g
  moss.png
  rook.g
  ${basic_view_test_ctrls}
  ${faceplate_test_ctrls}
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

