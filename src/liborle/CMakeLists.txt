#Include directories needed by liborle users
set(ORLE_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )
# local includes
set(ORLE_LOCAL_INCLUDE_DIRS ${UTAHRLE_INCLUDE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(orle ORLE_INCLUDE_DIRS ORLE_LOCAL_INCLUDE_DIRS)

BRLCAD_ADDLIB(liborle rle.c "NONE")
SET_TARGET_PROPERTIES(liborle PROPERTIES VERSION 19.0.1 SOVERSION 19)

ADD_MAN_PAGES(3 liborle.3)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
