
include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BRLCAD_TERMLIB_INCLUDE_DIR}
)

add_definitions(
	-DHAVE_CONFIG_H
	-DBRLCADBUILD
)

BRLCAD_ADDLIB(cursor cursor.c "${BRLCAD_TERMLIB_LIBRARY}")

install(FILES libcursor.3 DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man3)
