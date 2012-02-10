# Include directories needed by libmultispectral users
set(MULTISPECTRAL_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${RT_INCLUDE_DIRS}
  ${OPTICAL_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(multispectral MULTISPECTRAL_INCLUDE_DIRS "")

add_definitions(
  -DRT_MULTISPECTRAL
  )

BRLCAD_ADDLIB(libmultispectral "sh_temp.c" "librt librtms-static")
SET_TARGET_PROPERTIES(libmultispectral PROPERTIES VERSION 19.0.1 SOVERSION 19)
CMAKEFILES(Makefile.am)
