# Include directories needed by libbn users
set(BN_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(bn BN_INCLUDE_DIRS "")

set(LIBBN_SOURCES
  anim.c
  axis.c
  complex.c
  font.c
  globals.c
  list.c
  marker.c
  mat.c
  msr.c
  multipoly.c
  noise.c
  obr.c
  plane.c
  plot3.c
  poly.c
  qmath.c
  rand.c
  randmt.c
  scale.c
  sphmap.c
  symbol.c
  tabdata.c
  tcl.c
  tplot.c
  tri_tri.c
  ulp.c
  vectfont.c
  vector.c
  vers.c
  vert_tree.c
  wavelet.c
  )


BRLCAD_ADDLIB(libbn "${LIBBN_SOURCES}" libbu)
SET_TARGET_PROPERTIES(libbn PROPERTIES VERSION 20.0.1 SOVERSION 20)

BRLCAD_ADDEXEC(bntester bntester.c "libbu;libbn" NO_INSTALL)

add_subdirectory(tests)

CMAKEFILES(
  README
  bntester.dat
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
