if(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "-${CMAKE_CFG_INTDIR}")
else(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "")
endif(CMAKE_CONFIGURATION_TYPES)

macro(ADD_NURBS_NIRT_Test testname topts gfile targs depends_list)
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress${cfg_testname}-${testname}.log")
  set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${testname}${cfg_testname}.done")
  set(NHIT_OPTS "${topts}")
  set(NHIT_GFILE "${gfile}")
  set(NHIT_ARGS "${targs}")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-nurbs-nirt.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-nurbs-nirt-${testname}.cmake" @ONLY)
  add_custom_command(
    OUTPUT "${STAMP_FILE}"
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-nurbs-nirt-${testname}.cmake"
    DEPENDS ${depends_list}
    )
  add_test(NAME regress-nurbs-nirt-${testname} COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-nurbs-nirt-${testname}.cmake")
  add_custom_target(regress-nurbs-nirt-${testname} DEPENDS ${depends_list} ${STAMP_FILE})
  set_target_properties(regress-nurbs-nirt-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  Add_Regression_Test(nurbs-nirt-${testname} "${depends_list}")

  add_dependencies(regress-nurbs regress-nurbs-nirt-${testname})

  DISTCLEAN(
    ${STAMP_FILE}
    ${LOG_FILE}
    ${TARGET_IMAGE}
    )
endmacro()

add_custom_target(regress-nurbs)
set_target_properties(regress-nurbs PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(regress-nurbs PROPERTIES FOLDER "BRL-CAD Regression Tests")

ADD_NURBS_NIRT_Test(NIST03_MISS_01
  "-M -b"
  "${CMAKE_BINARY_DIR}/${DATA_DIR}/db/nist/NIST_MBE_PMI_3.g"
  "Document -114.61944548140064626 -127.49462006675899772 -134.83948901109894791 35 25"
  "analyze_nhit;NIST_MBE_PMI_3.g")

CMAKEFILES(
  brep-3dm.tar.bz2
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
