# Include directories needed by libgm users
set(GM_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BN_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(gm GM_INCLUDE_DIRS "")

set(LIBGM_SOURCES
  chull.c
  chull3d.cpp
  obr.c
  polygon.c
  tri_ray.c
  tri_tri.c
  util.c
  )

BRLCAD_ADDLIB(libgm "${LIBGM_SOURCES}" libbn libbu)
set_target_properties(libgm PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

CMAKEFILES(gm_private.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
