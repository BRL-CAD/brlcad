include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(remrt "ihost.c remrt.c" "libremrt liboptical libfb")
ADD_MAN_PAGE(1 remrt.1)

BRLCAD_ADDEXEC(rtsrv rtsrv.c "libremrt liboptical libpkg")
SET_TARGET_PROPERTIES(rtsrv PROPERTIES COMPILE_DEFINITIONS "RTSRV")

CMAKEFILES(ihost.h protocol.h)
