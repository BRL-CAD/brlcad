set(
  NIST_SAMPLE_MODELS
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
  g_target(${im} "BRL-CAD Geometry Models/Sample")
endforeach(im ${NIST_SAMPLE_MODELS})

# NIST 6 face 51 contains an unrelated seam excursion at a spherical pole.
# It must retain the native rigorous result without invoking mesh repair.
if(BUILD_TESTING AND TARGET brep-audit AND TARGET NIST_MBE_PMI_6.g)
  brlcad_add_test(
    NAME brep_cdt_nist6_native
    COMMAND
      brep-audit
      --mode
      quality
      --valid-solids-only
      --memory-limit-mib
      4096
      "${CMAKE_BINARY_DIR}/${DATA_DIR}/db/nist/NIST_MBE_PMI_6.g"
      Document_item.s
  )
  set_tests_properties(
    brep_cdt_nist6_native
    PROPERTIES
      REQUIRED_FILES "${CMAKE_BINARY_DIR}/${DATA_DIR}/db/nist/NIST_MBE_PMI_6.g"
      TIMEOUT 90
  )
  add_dependencies(check NIST_MBE_PMI_6.g)
  add_dependencies(unit NIST_MBE_PMI_6.g)
endif()

cmakefiles(
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
