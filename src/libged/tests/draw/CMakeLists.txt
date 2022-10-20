BRLCAD_ADDEXEC(ged_test_draw draw.cpp "libged;libdm" TEST)
BRLCAD_ADD_TEST(NAME ged_test_drawing COMMAND ged_test_draw "${CMAKE_CURRENT_SOURCE_DIR}/moss.g" "${CMAKE_CURRENT_SOURCE_DIR}")

CMAKEFILES(
  README
  empty.png
  moss.g
  v001_ctrl.png
  v002_ctrl.png
  v003_ctrl.png
  v004_ctrl.png
  v005_ctrl.png
  v006_ctrl.png
  v007_ctrl.png
  v008_ctrl.png
  v009_ctrl.png
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

