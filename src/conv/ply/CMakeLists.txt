set(PLY_INCLUDE_DIRS
  ${GCV_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(PLY_INCLUDE_DIRS)

BRLCAD_ADDEXEC(ply-g ply-g.c "libgcv" FOLDER Conv)
BRLCAD_ADDEXEC(g-ply g-ply.c "libgcv" FOLDER Conv)
add_dependencies(ply-g gcv_plugins)
add_dependencies(g-ply gcv_plugins)

CMAKEFILES(
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
