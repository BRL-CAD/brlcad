set(LIBMULTISPECTRAL_SRCS
  ../liboptical/init.c
  ../liboptical/material.c
  ../liboptical/refract.c
  ../liboptical/sh_camo.c
  ../liboptical/sh_light.c
  ../liboptical/sh_noise.c
  ../liboptical/sh_plastic.c
  ../liboptical/sh_stack.c
  ../liboptical/shade.c
  ../liboptical/wray.c
  sh_temp.c
  )

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

BRLCAD_ADDLIB(libmultispectral "${LIBMULTISPECTRAL_SRCS}" "librt;libbn;libbu")
SET_TARGET_PROPERTIES(libmultispectral PROPERTIES VERSION 19.0.1 SOVERSION 19)
CMAKEFILES(Makefile.am)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
