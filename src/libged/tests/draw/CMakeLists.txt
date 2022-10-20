BRLCAD_ADDEXEC(ged_test_draw draw.c "libged;libdm" TEST)
BRLCAD_ADD_TEST(NAME ged_test_draw_moss COMMAND ged_test_draw "${CMAKE_CURRENT_SOURCE_DIR}/moss.g" "${CMAKE_CURRENT_SOURCE_DIR}")

CMAKEFILES(
  empty.png
  moss.g
  v001_ctrl.png
  v002_ctrl.png
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

