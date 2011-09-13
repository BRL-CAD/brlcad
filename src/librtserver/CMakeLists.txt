include_directories(
	${TCL_INCLUDE_DIRS}
	)

BRLCAD_ADDLIB(rtserver rtserver.c "librt libbn libbu ${WINSOCK_LIB}")

add_executable(rtserverTest rtserverTest.c)
target_link_libraries(rtserverTest libbu rtserver)
CMAKEFILES(Makefile.am)
