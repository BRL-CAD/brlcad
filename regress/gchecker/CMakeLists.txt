if(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "-${CMAKE_CFG_INTDIR}")
else(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "")
endif(CMAKE_CONFIGURATION_TYPES)

set(GCHECKER_CLEAN_FILES
  gchecker_test.g.ck/ck.gchecker_test.g.overlaps
  gchecker.log
  )

set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress${cfg_testname}-gchecker.log")
set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/gchecker${cfg_testname}.done")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-gchecker.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-gchecker.cmake" @ONLY)
add_custom_command(
  OUTPUT "${STAMP_FILE}"
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-gchecker.cmake"
  DEPENDS ${depends_list}
  )
add_test(NAME regress-gchecker COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-gchecker.cmake")
add_custom_target(regress-gchecker DEPENDS gchecker mged gqa rtcheck "${STAMP_FILE}")
set_target_properties(regress-gchecker PROPERTIES FOLDER "BRL-CAD Regression Tests")
Add_Regression_Test(gchecker "gchecker;mged;gqa;rtcheck")

CMAKEFILES(
  CMakeLists.txt
  gchecker_test.g
  )

DISTCLEAN(
  ${STAMP_FILE}
  ${LOG_FILE}
  ${TARGET_IMAGE}
  )
foreach(GCF ${GCHECKER_CLEAN_FILES})
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${GCF}")
endforeach(GCF ${GCHECKER_CLEAN_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
