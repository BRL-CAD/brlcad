# Include directories needed by librtserver users
SET(RTSERVER_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BU_INCLUDE_DIRS}
	${BN_INCLUDE_DIRS}
	${RT_INCLUDE_DIRS}
	)
LIST(REMOVE_DUPLICATES RTSERVER_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(RTSERVER_INCLUDE_DIRS)


BRLCAD_ADDLIB(rtserver rtserver.c "librt libbn libbu ${WINSOCK_LIB}")

add_executable(rtserverTest rtserverTest.c)
target_link_libraries(rtserverTest libbu rtserver)
CMAKEFILES(Makefile.am)
