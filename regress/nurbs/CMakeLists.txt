# All NURBS ray tests will use the same template script
set(TSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-nurbs-test.cmake.in")

if (TARGET NIST_MBE_PMI_3.g)
  set(NHIT_NAME "NIST03_MISS_01")
  set(NHIT_OPTS "-M -b")
  set(NHIT_GFILE "${DATA_DIR}/db/nist/NIST_MBE_PMI_3.g")
  set(NHIT_ARGS "Document -114.61944548140064626 -127.49462006675899772 -134.83948901109894791 35 25")
  BRLCAD_REGRESSION_TEST(regress-nurbs_${NHIT_NAME} "analyze_nhit;NIST_MBE_PMI_3.g" TEST_SCRIPT "${TSCRIPT}" EXEC analyze_nhit)
  set_target_properties(regress-nurbs_${NHIT_NAME} PROPERTIES FOLDER "BRL-CAD Regression Tests/NURBS")
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/regress-nurbs_${NHIT_NAME}.log)
endif (TARGET NIST_MBE_PMI_3.g)


set(NHIT_NAME "PLATE_MODE_CYL_HIT_01")
set(NHIT_OPTS "-b")
set(NHIT_GFILE "${CMAKE_SOURCE_DIR}/src/libbrep/tests/plate_mode_samples.g")
set(NHIT_ARGS "sph.brep 0 0 0 0 0")
#BRLCAD_REGRESSION_TEST(regress-nurbs_${NHIT_NAME} "analyze_nhit" TEST_SCRIPT "${TSCRIPT}" EXEC analyze_nhit)
#set_target_properties(regress-nurbs-${NHIT_NAME} PROPERTIES FOLDER "BRL-CAD Regression Tests/NURBS")
#DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/regress-nurbs_${NHIT_NAME}.log)


set(NHIT_NAME "SPH_SOLID_HIT_LEN_01")
set(NHIT_OPTS "-L 2000 -b")
set(NHIT_GFILE "${CMAKE_CURRENT_SOURCE_DIR}/sph.g")
set(NHIT_ARGS "sph.brep 0 0 0 0 0")
BRLCAD_REGRESSION_TEST(regress-nurbs_${NHIT_NAME} "analyze_nhit" TEST_SCRIPT "${TSCRIPT}" EXEC analyze_nhit)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/regress-nurbs_${NHIT_NAME}.log)


set(NHIT_NAME "BOX_PLATE_HIT_LEN_01")
set(NHIT_OPTS "-L .1 -b")
set(NHIT_GFILE "${CMAKE_SOURCE_DIR}/src/libbrep/tests/plate_mode_samples.g")
set(NHIT_ARGS "box.s 0 0 0 270 90")
BRLCAD_REGRESSION_TEST(regress-nurbs_${NHIT_NAME} "analyze_nhit" TEST_SCRIPT "${TSCRIPT}" EXEC analyze_nhit)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/regress-nurbs_${NHIT_NAME}.log)

CMAKEFILES(
  CMakeLists.txt
  brep-3dm.tar.bz2
  regress-nurbs-test.cmake.in
  sph.g
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
