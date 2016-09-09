set(naca_srcs
  splprocs.c
  epspsi.c
  nacax.c
  naca456.c
  )

set(NACA_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OPENNURBS_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(NACA_INCLUDES)

BRLCAD_ADDEXEC(naca456 "${naca_srcs}" "libwdb;${OPENNURBS_LIBRARIES}" NO_INSTALL)

CMAKEFILES(naca.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
