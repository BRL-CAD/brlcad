set(GCV_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${GCV_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(GCV_INCLUDE_DIRS)


BRLCAD_ADDEXEC(gcv gcv.c "libgcv;libbn;libbu;${M_LIBRARY}")
add_dependencies(gcv gcv_plugins)

CMAKEFILES(gcv.sh)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
