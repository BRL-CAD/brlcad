include_directories(
	${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(fbserv "fbserv.c server.c" libfb)

install(FILES fbserv.1 DESTINATION ${MAN_DIR}/man1)
