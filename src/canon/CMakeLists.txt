include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(canonize "canonize.c canonlib.c" libfb)
install(FILES canonize.1 DESTINATION ${MAN_DIR}/man1)
