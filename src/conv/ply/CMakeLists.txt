set(PLY_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(PLY_INCLUDE_DIRS)

set(ply-g_SRCS
  rply.c
  ply-g.c
  )

set(g-ply_SRCS
  rply.c
  g-ply.c
  )


BRLCAD_ADDEXEC(ply-g "${ply-g_SRCS}" "librt;libbu;libwdb")
BRLCAD_ADDEXEC(g-ply "${g-ply_SRCS}" "librt;libbu")

CMAKEFILES(rply.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

