set(naca_srcs
  splprocs.c
  epspsi.c
  naca456.c
  )

BRLCAD_ADDEXEC(naca456 "${naca_srcs}" "libwdb;${OPENNURBS_LIBRARY}" NO_STRICT NO_INSTALL)

CMAKEFILES(naca.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
