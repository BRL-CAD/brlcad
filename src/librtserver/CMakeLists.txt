include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${TCL_INCLUDE_PATH}
	${JAVA_INCLUDE_PATH}
	)

add_definitions(
	-DHAVE_CONFIG_H
	-DBRLCADBUILD
	)

BRLCAD_ADDLIB(rtserver rtserver.c "bu bn librt")

add_executable(rtserverTest rtserverTest.c)
target_link_libraries(rtserverTest bu rtserver)
