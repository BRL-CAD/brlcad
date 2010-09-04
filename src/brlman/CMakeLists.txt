SET(BRLCAD_ROOT ${BRLCAD_PREFIX})

configure_file(brlman.sh.in ${CMAKE_BINARY_DIR}/src/brlman/brlman)
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/src/brlman/brlman DESTINATION ${BRLCAD_INSTALL_BIN_DIR})
