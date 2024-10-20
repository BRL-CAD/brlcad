set(
  LIBBN_SOURCES
  anim.c
  complex.c
  globals.c
  mat.c
  msr.c
  mt19937ar.c
  multipoly.c
  noise.c
  poly.c
  qmath.c
  rand.c
  randsph.c
  sobol.c
  sphmap.c
  str.c
  tabdata.c
  ulp.c
  vers.c
  wavelet.c
)

set(BN_LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# Note - libbn_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
brlcad_addlib(libbn "${LIBBN_SOURCES}" "${libbn_deps}" "" "${BN_LOCAL_INCLUDE_DIRS}")
set_target_properties(libbn PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

cmakefiles(
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
