include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(vdeck "cgarbs.c match.c parsarg.c vdeck.c" librt)
ADD_MAN_PAGE(1 vdeck.1)
CMAKEFILES(std.h vextern.h)
