# Include directories needed by librtserver users
set(RTSERVER_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BU_INCLUDE_DIRS}
	${BN_INCLUDE_DIRS}
	${RT_INCLUDE_DIRS}
	${JNI_INCLUDE_DIRS}
	)
BRLCAD_INCLUDE_DIRS(RTSERVER_INCLUDE_DIRS)

BRLCAD_ADDLIB(rtserver rtserver.c "librt libbn libbu ${WINSOCK_LIB} ${ZLIB_LIBRARY}")
set_target_properties(rtserver PROPERTIES VERSION 20.0.1 SOVERSION 20)
set_target_properties(rtserver PROPERTIES SUFFIX ".jnilib")

add_executable(rtserverTest rtserverTest.c)
target_link_libraries(rtserverTest libbu rtserver)
CMAKEFILES(Makefile.am)
