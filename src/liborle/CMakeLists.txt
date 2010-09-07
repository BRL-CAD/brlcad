set(LIBORLE_SOURCES
    rle.c
)

include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BRLCAD_UTAHRLE_INCLUDE_DIR}
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

BRLCAD_ADDLIB(orle "${LIBORLE_SOURCES}" "NONE")

install(FILES liborle.3 DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man3)
