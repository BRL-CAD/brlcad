# Include directories needed by libbg users
set(BG_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BN_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(bg BG_INCLUDE_DIRS "")

set(LIBBG_SOURCES
  chull.c
  chull3d.cpp
  obr.c
  polygon.c
  tri_ray.c
  tri_tri.c
  util.c
  )

BRLCAD_ADDLIB(libbg "${LIBBG_SOURCES}" libbn libbu)
set_target_properties(libbg PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

CMAKEFILES(bg_private.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
