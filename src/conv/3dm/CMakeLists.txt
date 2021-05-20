set(3DM_INCLUDE_DIRS
  ${GCV_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(3DM_INCLUDE_DIRS)

BRLCAD_ADDEXEC(3dm-g 3dm-g.c "libgcv" FOLDER Conv)
add_dependencies(3dm-g gcv_plugins)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
