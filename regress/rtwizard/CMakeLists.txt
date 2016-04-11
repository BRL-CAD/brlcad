# Tests for the various image format conversions supported
# by BRL-CAD

# For multi-config build tools
if(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "-${CMAKE_CFG_INTDIR}")
else(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "")
endif(CMAKE_CONFIGURATION_TYPES)

# TODO: Hack to avoid running multiple rtwizard tests at once.  Correct
# solution is to fix this, but until then make new targets depend on old
# targets.  This is also why the Add_Regression_Test command below sets
# the EXCLUDE_FROM_REGRESS flag - a parallel distcheck may run multiple
# rtwizards at the same time - each individual distcheck has no way to
# know what other distchecks are doing.
set(rtwizard_targets)

macro(Add_RtWiz_Test testname input_file options depends_list)
  CMAKE_PARSE_ARGUMENTS(${testname} "" "CONTROL_IMAGE;TARGET_IMAGE;LOG_FILE" "" ${ARGN})
  set(TARGET_NAME ${testname})
  set(OPTIONS ${options})
  set(INPUT_FILE ${input_file})
  set(CONTROL_IMAGE ${${testname}_CONTROL_IMAGE})
  set(TARGET_IMAGE ${${testname}_TARGET_IMAGE})
  set(LOG_FILE ${${testname}_LOG_FILE})
  set(TARGET_NAME ${testname})
  set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${testname}${cfg_testname}.done")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-rtwiz.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake" @ONLY)
  set (dep_list pixcmp rtwizard ${depends_list})
  add_custom_command(
    OUTPUT ${STAMP_FILE} ${TARGET_IMAGE} ${LOG_FILE}
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake"
    DEPENDS ${dep_list}
    )

  add_test(NAME regress-rtwizard-${testname} COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake")
  add_custom_target(regress-rtwizard-${testname} DEPENDS ${dep_list} ${STAMP_FILE})
  set_target_properties(regress-rtwizard-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  Add_Regression_Test(rtwizard-${testname} "${dep_list}" EXCLUDE_FROM_REGRESS)

  foreach(tgt ${rtwizard_targets})
    add_dependencies(regress-rtwizard-${testname} ${tgt})
    set_property(TEST regress-rtwizard-${testname} APPEND PROPERTY DEPENDS ${tgt})
  endforeach(tgt ${rtwizard_targets})

  set(rtwizard_targets ${rtwizard_targets} regress-rtwizard-${testname})

  add_dependencies(regress-rtwizard regress-rtwizard-${testname})

  DISTCLEAN(
    ${STAMP_FILE}
    ${LOG_FILE}
    ${TARGET_IMAGE}
    )
endmacro()

add_custom_target(regress-rtwizard)
set_target_properties(regress-rtwizard PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(regress-rtwizard PROPERTIES FOLDER "BRL-CAD Regression Tests")

# Unpack compressed reference images
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35_rtwiz.tar.bz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_A_ref.pix")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_B_ref.pix")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_C_ref.pix")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_D_ref.pix")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_E_ref.pix")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_F_ref.pix")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_ref.pix")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_color_ref.pix")

Add_RtWiz_Test(rtwiz_m35_A "${DATA_DIR}/db/m35.g" "all.g -t A --benchmark -s 512" "m35.g"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_A_ref.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_A.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_A.log"
  )

Add_RtWiz_Test(rtwiz_m35_B "${DATA_DIR}/db/m35.g" "-l component -t B --benchmark -z 2" "m35.g"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_B_ref.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_B.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_B.log"
  )

Add_RtWiz_Test(rtwiz_m35_C "${DATA_DIR}/db/m35.g" "-c component -l component -t C --benchmark -z 2" "m35.g"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_C_ref.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_C.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_C.log"
  )

Add_RtWiz_Test(rtwiz_m35_D "${DATA_DIR}/db/m35.g" "-l component -c component/suspension,component/power.train -t D --benchmark -z 2" "m35.g"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_D_ref.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_D.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_D.log"
  )

Add_RtWiz_Test(rtwiz_m35_E "${DATA_DIR}/db/m35.g" "-g component -c component/power.train -t E --benchmark -z 2" "m35.g"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_E_ref.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_E.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_E.log"
  )

Add_RtWiz_Test(rtwiz_m35_F "${DATA_DIR}/db/m35.g" "-l component -g component -c component/power.train -t F --benchmark -z 2" "m35.g"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_F_ref.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_F.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_F.log"
  )

Add_RtWiz_Test(rtwiz_m35_edge_only "${DATA_DIR}/db/m35.g" "-l component --benchmark -z 2" "m35.g"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_ref.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only.log"
  )

Add_RtWiz_Test(rtwiz_m35_edge_only_color "${DATA_DIR}/db/m35.g" "-l component --benchmark -z 2 --line-color 100/200/255 -C 30/30/30" "m35.g"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_color_ref.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_color.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_color.log"
  )

CMAKEFILES(
  m35_rtwiz.tar.bz2
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
