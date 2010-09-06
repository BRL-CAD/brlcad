include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_STD_INCLUDES}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

BRLCAD_ADDEXEC(canonize "canonize.c canonlib.c" "fb")
install(FILES canonize.1 DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
