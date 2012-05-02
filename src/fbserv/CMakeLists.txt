include_directories(
  ${FB_INCLUDE_DIRS}
  )

BRLCAD_ADDEXEC(fbserv "fbserv.c;server.c" libfb)

ADD_MAN_PAGES(1 fbserv.1)
CMAKEFILES(Makefile.am)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
