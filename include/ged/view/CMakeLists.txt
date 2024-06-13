set(ged_view_headers
  matrix.h
  select.h
  state.h
  )
BRLCAD_MANAGE_FILES(ged_view_headers ${INCLUDE_DIR}/brlcad/ged/view REQUIRED libged)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
