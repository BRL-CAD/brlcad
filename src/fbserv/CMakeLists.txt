
include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${TCL_INCLUDE_PATH}
)

add_definitions(
	-DHAVE_CONFIG_H
	-DBRLCADBUILD
)

add_executable(fbserv fbserv.c server.c)
target_link_libraries(fbserv fb)
INSTALL(TARGETS fbserv RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

