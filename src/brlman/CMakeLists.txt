SET(BRLCAD_ROOT "${CMAKE_INSTALL_PREFIX}")
SET(BRLCAD_DATA "${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}")
SET(prefix "${CMAKE_INSTALL_PREFIX}")
SET(datarootdir "\${prefix}/share")
SET(mandir "\${datarootdir}/man")

configure_file(brlman.sh.in ${CMAKE_BINARY_DIR}/src/brlman/brlman @ONLY)
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/src/brlman/brlman DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_BIN_DIR})
install(FILES brlman.1 DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR}/man1)
