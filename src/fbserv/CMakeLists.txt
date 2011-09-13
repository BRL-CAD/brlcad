include_directories(
	${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(fbserv "fbserv.c server.c" libfb)

ADD_MAN_PAGE(1 fbserv.1)
CMAKEFILES(Makefile.am)
