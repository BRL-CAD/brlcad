set(bn_headers
  adc.h
  anim.h
  clip.h
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
  str.h
  tol.h
  vectfont.h
  vector_fpu.h
  vector_x86.h
  version.h
  vert_tree.h
  vlist.h
  wavelet.h
  )

install(FILES ${bn_headers} DESTINATION ${INCLUDE_DIR}/brlcad/bn)

CMAKEFILES(${bn_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
