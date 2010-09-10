set(LIBPKG_SOURCES
    pkg.c
    tcl.c
    vers.c
)

INSTALL(FILES tpkg.c DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/sample_applications)

include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${TCL_INCLUDE_PATH}
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

BRLCAD_ADDLIB(pkg "${LIBPKG_SOURCES}" bu)
SET_TARGET_PROPERTIES(pkg PROPERTIES VERSION 19.0.1 SOVERSION 19)
