include_directories(
    ${TCL_INCLUDE_DIRS}
)

set(LIBPKG_SOURCES
    pkg.c
    tcl.c
    vers.c
)

FILE(COPY tpkg.c DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/sample_applications)
INSTALL(FILES tpkg.c DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/sample_applications)

BRLCAD_ADDLIB(libpkg "${LIBPKG_SOURCES}" libbu STRICT)
SET_TARGET_PROPERTIES(libpkg PROPERTIES VERSION 19.0.1 SOVERSION 19)
