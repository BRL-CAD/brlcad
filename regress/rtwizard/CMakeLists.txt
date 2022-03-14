# Tests for image generation via rtwizard

if (TARGET rtwizard)

# Note: we have to set up test dependencies to avoid running multiple rtwizard
# tests at once.  Correct solution is to fix the weaknesses of rtwizard that
# prevent its being run in parallel, but that will be a lot of work and until then we
# make each image test depend on the previous test.
#
# This is also why these tests must be STAND_ALONE tests - a parallel distcheck
# running multiple simultaneous builds may run multiple rtwizards at the same
# time, since the below dependencies are only valid within a single build -
# each individual distcheck has no way to know what other distchecks are doing.

# Since these tests can't currently be part of the main regress target, we define
# convenience target to run all of the rtwizard tests:
if (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-rtwizard COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_CFG_INTDIR} -L STAND_ALONE -R regress-rtwiz --output-on-failure --output-log "${CMAKE_BINARY_DIR}/regress-rtwizard_output.log")
else (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-rtwizard COMMAND ${CMAKE_CTEST_COMMAND} -L STAND_ALONE -R regress-rtwiz --output-on-failure --output-log "${CMAKE_BINARY_DIR}/regress-rtwizard_output.log")
endif (CMAKE_CONFIGURATION_TYPES)
set_target_properties(regress-rtwizard PROPERTIES FOLDER "BRL-CAD Regression Tests/rtwizard")

# This target also acts like the check target in that it depends on the
# necessary inputs for the CTest tests
add_dependencies(regress-rtwizard rtwizard pixcmp m35.g)

# All rtwizard tests used the same script
set(TSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-rtwiz.cmake.in")

set(INPUT_FILE "${DATA_DIR}/db/m35.g")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_A_ref.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_A.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_A.log")
set(OPTIONS "all.g -t A --benchmark -s 512")
BRLCAD_REGRESSION_TEST(regress-rtwiz_m35_A "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard STAND_ALONE)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

set(INPUT_FILE "${DATA_DIR}/db/m35.g")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_B_ref.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_B.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_B.log")
set(OPTIONS "-l component -t B --benchmark -z 2")
BRLCAD_REGRESSION_TEST(regress-rtwiz_m35_B "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard STAND_ALONE)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})
set_property(TEST regress-rtwiz_m35_B APPEND PROPERTY DEPENDS regress-rtwiz_m35_A)
add_dependencies(regress-rtwiz_m35_B regress-rtwiz_m35_A)

set(INPUT_FILE "${DATA_DIR}/db/m35.g")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_C_ref.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_C.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_C.log")
set(OPTIONS "-c component -l component -t C --benchmark -z 2")
BRLCAD_REGRESSION_TEST(regress-rtwiz_m35_C "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard STAND_ALONE)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})
set_property(TEST regress-rtwiz_m35_C APPEND PROPERTY DEPENDS regress-rtwiz_m35_B)
add_dependencies(regress-rtwiz_m35_C regress-rtwiz_m35_B)

set(INPUT_FILE "${DATA_DIR}/db/m35.g")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_D_ref.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_D.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_D.log")
set(OPTIONS "-l component -c component/suspension,component/power.train -t D --benchmark -z 2")
BRLCAD_REGRESSION_TEST(regress-rtwiz_m35_D "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard STAND_ALONE)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})
set_property(TEST regress-rtwiz_m35_D APPEND PROPERTY DEPENDS regress-rtwiz_m35_C)
add_dependencies(regress-rtwiz_m35_D regress-rtwiz_m35_C)

set(INPUT_FILE "${DATA_DIR}/db/m35.g")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_E_ref.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_E.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_E.log")
set(OPTIONS "-g component -c component/power.train -t E --benchmark -z 2")
BRLCAD_REGRESSION_TEST(regress-rtwiz_m35_E "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard STAND_ALONE)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})
set_property(TEST regress-rtwiz_m35_E APPEND PROPERTY DEPENDS regress-rtwiz_m35_D)
add_dependencies(regress-rtwiz_m35_E regress-rtwiz_m35_D)

set(INPUT_FILE "${DATA_DIR}/db/m35.g")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_F_ref.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_F.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_F.log")
set(OPTIONS "-l component -g component -c component/power.train -t F --benchmark -z 2")
BRLCAD_REGRESSION_TEST(regress-rtwiz_m35_F "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard STAND_ALONE)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})
set_property(TEST regress-rtwiz_m35_F APPEND PROPERTY DEPENDS regress-rtwiz_m35_E)
add_dependencies(regress-rtwiz_m35_F regress-rtwiz_m35_E)

set(INPUT_FILE "${DATA_DIR}/db/m35.g")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_edge_only_ref.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only.log")
set(OPTIONS "-l component --benchmark -z 2")
BRLCAD_REGRESSION_TEST(regress-rtwiz_m35_edge_only "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard STAND_ALONE)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})
set_property(TEST regress-rtwiz_m35_edge_only APPEND PROPERTY DEPENDS regress-rtwiz_m35_F)
add_dependencies(regress-rtwiz_m35_edge_only regress-rtwiz_m35_F)

set(INPUT_FILE "${DATA_DIR}/db/m35.g")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_edge_only_color_ref.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_color.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_color.log")
set(OPTIONS "-l component --benchmark -z 2 --line-color 100/200/255 -C 30/30/30")
BRLCAD_REGRESSION_TEST(regress-rtwiz_m35_edge_only_color "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard STAND_ALONE)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})
set_property(TEST regress-rtwiz_m35_edge_only_color APPEND PROPERTY DEPENDS regress-rtwiz_m35_edge_only)
add_dependencies(regress-rtwiz_m35_edge_only_color regress-rtwiz_m35_edge_only)

endif (TARGET rtwizard)

CMAKEFILES(
  CMakeLists.txt
  m35_A_ref.pix
  m35_B_ref.pix
  m35_C_ref.pix
  m35_D_ref.pix
  m35_E_ref.pix
  m35_F_ref.pix
  m35_edge_only_color_ref.pix
  m35_edge_only_ref.pix
  regress-rtwiz.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
