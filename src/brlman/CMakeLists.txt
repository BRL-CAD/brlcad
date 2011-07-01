SET(BRLCAD_ROOT "${CMAKE_BINARY_DIR}")
SET(BRLCAD_DATA "${DATA_DIR}")
SET(prefix "${CMAKE_BINARY_DIR}")
SET(datarootdir "\${prefix}/share")
SET(mandir "\${datarootdir}/man")
configure_file(brlman.sh.in ${CMAKE_BINARY_DIR}/src/brlman/brlman @ONLY)
file(COPY ${CMAKE_BINARY_DIR}/src/brlman/brlman
	DESTINATION ${CMAKE_BINARY_DIR}/bin
	FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
	GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
FILE(REMOVE ${CMAKE_BINARY_DIR}/src/brlman/brlman)

SET(BRLCAD_ROOT "${CMAKE_INSTALL_PREFIX}")
SET(BRLCAD_DATA "${DATA_DIR}")
SET(prefix "${CMAKE_INSTALL_PREFIX}")
SET(datarootdir "\${prefix}/share")
SET(mandir "\${datarootdir}/man")

configure_file(brlman.sh.in ${CMAKE_BINARY_DIR}/src/brlman/brlman @ONLY)
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/src/brlman/brlman DESTINATION ${BIN_DIR})
install(FILES brlman.1 DESTINATION ${MAN_DIR}/man1)
