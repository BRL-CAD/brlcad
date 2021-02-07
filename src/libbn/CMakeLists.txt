# Include directories needed by libbn users
set(BN_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  )

set(BN_LOCAL_INCLUDE_DIRS
  ${BRLCAD_SOURCE_DIR}/src/other/Eigen
  )

BRLCAD_LIB_INCLUDE_DIRS(bn BN_INCLUDE_DIRS BN_LOCAL_INCLUDE_DIRS)

set(LIBBN_SOURCES
  anim.c
  axis.c
  complex.c
  clip.c
  font.c
  globals.c
  list.c
  marker.c
  mat.c
  msr.c
  mt19937ar.c
  multipoly.c
  noise.c
  plane.cpp
  plot3.c
  poly.c
  qmath.c
  rand.c
  randsph.c
  scale.c
  sobol.c
  sphmap.c
  str.c
  symbol.c
  tabdata.c
  tplot.c
  ulp.c
  vectfont.c
  vector.c
  vers.c
  vert_tree.c
  vlist.c
  wavelet.c
  )


BRLCAD_ADDLIB(libbn "${LIBBN_SOURCES}" "libbu")
set_target_properties(libbn PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

CMAKEFILES(
  CMakeLists.txt
  README
  soboldata.h
  numgen.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
