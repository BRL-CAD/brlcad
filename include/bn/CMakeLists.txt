set(bn_headers
  adc.h
  anim.h
  complex.h
  defines.h
  dvec.h
  mat.h
  msr.h
  multipoly.h
  noise.h
  plane.h
  plot3.h
  poly.h
  qmath.h
  rand.h
  randmt.h
  spm.h
  tabdata.h
  sobol.h
  str.h
  tig.h
  tol.h
  vectfont.h
  version.h
  vert_tree.h
  vlist.h
  wavelet.h
  )
BRLCAD_MANAGE_FILES(bn_headers ${INCLUDE_DIR}/brlcad/bn)

CMAKEFILES(numgen.h)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
