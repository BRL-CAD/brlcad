set(analyze_headers
  debug.h
  defines.h
  density.h
  diff.h
  grid.h
  heal.h
  info.h
  nirt.h
  pnts.h
  polygonize.h
  voxelize.h
  worker.h
  )
BRLCAD_MANAGE_FILES(analyze_headers ${INCLUDE_DIR}/brlcad/analyze)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
