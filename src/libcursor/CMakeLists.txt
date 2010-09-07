
include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BRLCAD_TERMLIB_INCLUDE_DIR}
)

add_definitions(
	-DHAVE_CONFIG_H
	-DBRLCADBUILD
)

add_library(cursor cursor.c)
target_link_libraries(cursor ${BRLCAD_TERMLIB_LIBRARY})
INSTALL(TARGETS cursor DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

install(FILES libcursor.3 DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man3)
