include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(canonize "canonize.c canonlib.c" libfb)
ADD_MAN_PAGE(1 canonize.1)
CMAKEFILES(canon.h chore.h)
