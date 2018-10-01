set(NIST_SAMPLE_MODELS
  NIST_MBE_PMI_1.stp
  NIST_MBE_PMI_2.stp
  NIST_MBE_PMI_3.stp
  NIST_MBE_PMI_4.stp
  NIST_MBE_PMI_5.stp
  NIST_MBE_PMI_6.stp
  NIST_MBE_PMI_7-10.3dm
  NIST_MBE_PMI_11.stp
)

foreach(im ${NIST_SAMPLE_MODELS})
  ADD_G_TARGET(${im} "BRL-CAD Geometry Models/Sample")
endforeach(im ${NIST_SAMPLE_MODELS})

CMAKEFILES(
  ${NIST_SAMPLE_MODELS}
  CMakeLists.txt
  README
  pdf/1.pdf
  pdf/2.pdf
  pdf/3.pdf
  pdf/4.pdf
  pdf/5.pdf
  pdf/6.pdf
  pdf/7-10_assem.pdf
  pdf/7.pdf
  pdf/8.pdf
  pdf/9.pdf
  pdf/10.pdf
  pdf/11.pdf
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
