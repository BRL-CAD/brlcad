set(bn_headers
  adc.h
  anim.h
  clip.h
  complex.h
  defines.h
  mat.h
  msr.h
  multipoly.h
  noise.h
  plane_calc.h
  plane_struct.h
  plot3.h
  poly.h
  qmath.h
  rand.h
  randmt.h
  tabdata.h
  bn_tcl.h
  tol.h
  vectfont.h
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
