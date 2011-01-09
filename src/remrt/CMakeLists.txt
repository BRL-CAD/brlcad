include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(remrt "ihost.c remrt.c" "libremrt liboptical libfb")
install(FILES remrt.1 DESTINATION ${MAN_DIR}/man1)

BRLCAD_ADDEXEC(rtsrv rtsrv.c "libremrt liboptical libpkg")
SET_TARGET_PROPERTIES(rtsrv PROPERTIES COMPILE_DEFINITIONS "RTSRV")
