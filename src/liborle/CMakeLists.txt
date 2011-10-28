#Include directories needed by liborle users
SET(ORLE_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	)
BRLCAD_INCLUDE_DIRS(ORLE_INCLUDE_DIRS)

# local includes
include_directories(${UTAHRLE_INCLUDE_DIR})

set(LIBORLE_SOURCES rle.c)

BRLCAD_ADDLIB(liborle "${LIBORLE_SOURCES}" "NONE")
SET_TARGET_PROPERTIES(liborle PROPERTIES VERSION 19.0.1 SOVERSION 19)

ADD_MAN_PAGE(3 liborle.3)
CMAKEFILES(Makefile.am)
