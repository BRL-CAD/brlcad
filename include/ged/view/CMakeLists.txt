set(ged_view_headers ged_view_tmp.h matrix.h select.h state.h)
brlcad_manage_files(ged_view_headers ${INCLUDE_DIR}/brlcad/ged/view REQUIRED libged)

cmakefiles(
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
