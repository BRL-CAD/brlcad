set(
  ged_headers
  bsg_ged_draw.h
  commands.h
  database.h
  db_index.h
  debug.h
  defines.h
  event_txn.h
  objects.h
  selection_state.h
  version.h
  view.h
)
brlcad_manage_files(ged_headers ${INCLUDE_DIR}/brlcad/ged REQUIRED libged)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
