include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(vdeck "cgarbs.c match.c parsarg.c vdeck.c" librt STRICT)
install(FILES vdeck.1 DESTINATION ${MAN_DIR}/man1)
