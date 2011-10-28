include_directories(${FB_INCLUDE_DIRS})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
)

BRLCAD_ADDEXEC(canonize "canonize.c canonlib.c" libfb)
ADD_MAN_PAGE(1 canonize.1)
CMAKEFILES(canon.h chore.h)
CMAKEFILES(Makefile.am)
