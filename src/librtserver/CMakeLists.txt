include_directories(
	${TCL_INCLUDE_PATH}
	)

BRLCAD_ADDLIB(rtserver rtserver.c "libbu libbn librt")

add_executable(rtserverTest rtserverTest.c)
target_link_libraries(rtserverTest libbu rtserver)
