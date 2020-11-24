set(3DM_INCLUDE_DIRS
  ${GCV_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(3DM_INCLUDE_DIRS)

BRLCAD_ADDEXEC(3dm-g 3dm-g.c "libgcv")
add_dependencies(3dm-g libgcv_plugins)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
