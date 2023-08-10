set(test_deps
  dm-swrast
  ged-autoview
  ged-close
  ged-draw
  ged-dm
  ged-facetize
  ged-open
  ged-png2fb
  ged-select
  ged-tol
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
BRLCAD_ADDEXEC(ged_test_draw "draw.cpp;util.cpp" "libged;libdm" TEST)
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
BRLCAD_ADDEXEC(ged_test_faceplate "faceplate.cpp;util.cpp" "libged;libdm" TEST)
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
BRLCAD_ADDEXEC(ged_test_lod "lod.cpp;util.cpp" "libged;libdm" TEST)
add_dependencies(ged_test_lod ${test_deps})
BRLCAD_ADD_TEST(NAME ged_test_drawing_lod COMMAND ged_test_lod "${CMAKE_CURRENT_SOURCE_DIR}")


set(select_test_ctrls
  select001_ctrl.png
  select002_ctrl.png
  select003_ctrl.png
  select004_ctrl.png
  select005_ctrl.png
  select006_ctrl.png
  select007_ctrl.png
  select008_ctrl.png
  select009_ctrl.png
  select010_ctrl.png
  select011_ctrl.png
  select012_ctrl.png
  select013_ctrl.png
  select014_ctrl.png
  select015_ctrl.png
  select016_ctrl.png
  select017_ctrl.png
  select018_ctrl.png
  select019_ctrl.png
  select020_ctrl.png
  select021_ctrl.png
  select022_ctrl.png
  select023_ctrl.png
  select024_ctrl.png
  select001_ctrl.png
  select002_ctrl.png
  select003_ctrl.png
  select004_ctrl.png
  select005_ctrl.png
  select006_ctrl.png
  select007_ctrl.png
  select008_ctrl.png
  select009_ctrl.png
  select010_ctrl.png
  select011_ctrl.png
  select012_ctrl.png
  select013_ctrl.png
  select019_ctrl.png
  select020_ctrl.png
  select021_ctrl.png
  select022_ctrl.png
  select023_ctrl.png
  select024_ctrl.png
  )
BRLCAD_ADDEXEC(ged_test_select_draw "select.cpp;util.cpp" "libged;libdm" TEST)
add_dependencies(ged_test_select_draw ${test_deps})
BRLCAD_ADD_TEST(NAME ged_test_drawing_select COMMAND ged_test_select_draw "${CMAKE_CURRENT_SOURCE_DIR}")

set(quad_test_ctrls
  quad_00_001_ctrl.png
  quad_00_002_ctrl.png
  quad_00_003_ctrl.png
  quad_00_004_ctrl.png
  quad_00_005_ctrl.png
  quad_00_006_ctrl.png
  quad_00_007_ctrl.png
  quad_00_008_ctrl.png
  quad_01_001_ctrl.png
  quad_01_002_ctrl.png
  quad_01_003_ctrl.png
  quad_01_004_ctrl.png
  quad_01_005_ctrl.png
  quad_01_006_ctrl.png
  quad_01_007_ctrl.png
  quad_01_008_ctrl.png
  quad_02_001_ctrl.png
  quad_02_002_ctrl.png
  quad_02_003_ctrl.png
  quad_02_004_ctrl.png
  quad_02_005_ctrl.png
  quad_02_006_ctrl.png
  quad_02_007_ctrl.png
  quad_02_008_ctrl.png
  quad_03_001_ctrl.png
  quad_03_002_ctrl.png
  quad_03_003_ctrl.png
  quad_03_004_ctrl.png
  quad_03_005_ctrl.png
  quad_03_006_ctrl.png
  quad_03_007_ctrl.png
  quad_03_008_ctrl.png
  )
BRLCAD_ADDEXEC(ged_test_quad quad.cpp "libged;libdm" TEST)
add_dependencies(ged_test_quad ${test_deps})
BRLCAD_ADD_TEST(NAME ged_test_drawing_quad COMMAND ged_test_quad "${CMAKE_CURRENT_SOURCE_DIR}")

# The idea here is to test view stability across resize events
set(aet_test_ctrls
  aet_00_001_ctrl.png
  aet_00_002_ctrl.png
  aet_01_001_ctrl.png
  aet_01_002_ctrl.png
  aet_02_001_ctrl.png
  aet_02_002_ctrl.png
  aet_03_001_ctrl.png
  aet_03_002_ctrl.png
  )
BRLCAD_ADDEXEC(ged_test_aet "aet.cpp;util.cpp" "libged;libdm" TEST)
add_dependencies(ged_test_aet ${test_deps})

CMAKEFILES(
  README
  empty.png
  moss.g
  moss.png
  rook.g
  util.cpp
  ${basic_view_test_ctrls}
  ${faceplate_test_ctrls}
  ${lod_test_ctrls}
  ${select_test_ctrls}
  ${quad_test_ctrls}
  ${aet_test_ctrls}
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

