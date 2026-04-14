# Tests for image generation via rtwizard

if(TARGET rtwizard)
  # rtwizard tests were historically run STAND_ALONE and forced sequential
  # because simultaneous instances would accidentally connect to each
  # other's fbserv (wrong images, cleared framebuffers).
  #
  # Each rtwizard invocation now generates a unique FBSERV_TOKEN and
  # launches fbserv with -A (strict auth), so parallel runs are fully
  # isolated.  Port detection uses a non-destructive TCP probe instead of
  # fbclear, eliminating the destructive side-effect that corrupted images
  # in competing instances.  The STAND_ALONE label and the sequential
  # DEPENDS chains are therefore no longer needed.

  # Convenience target to run only the rtwizard regression tests:
  add_custom_target(
    regress-rtwizard
    COMMAND
      ${CMAKE_CTEST_COMMAND} -L Regression -R regress-rtwiz --output-on-failure --output-log
      "${CMAKE_BINARY_DIR}/regress-rtwizard_output.log"
  )
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
  brlcad_regression_test(regress-rtwiz_m35_A "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard)
  distclean(${LOG_FILE} ${TARGET_IMAGE})

  set(INPUT_FILE "${DATA_DIR}/db/m35.g")
  set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_B_ref.pix")
  set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_B.pix")
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_B.log")
  set(OPTIONS "-l component -t B --benchmark -z 2")
  brlcad_regression_test(regress-rtwiz_m35_B "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard)
  distclean(${LOG_FILE} ${TARGET_IMAGE})

  set(INPUT_FILE "${DATA_DIR}/db/m35.g")
  set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_C_ref.pix")
  set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_C.pix")
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_C.log")
  set(OPTIONS "-c component -l component -t C --benchmark -z 2")
  brlcad_regression_test(regress-rtwiz_m35_C "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard)
  distclean(${LOG_FILE} ${TARGET_IMAGE})

  set(INPUT_FILE "${DATA_DIR}/db/m35.g")
  set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_D_ref.pix")
  set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_D.pix")
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_D.log")
  set(OPTIONS "-l component -c component/suspension,component/power.train -t D --benchmark -z 2")
  brlcad_regression_test(regress-rtwiz_m35_D "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard)
  distclean(${LOG_FILE} ${TARGET_IMAGE})

  set(INPUT_FILE "${DATA_DIR}/db/m35.g")
  set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_E_ref.pix")
  set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_E.pix")
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_E.log")
  set(OPTIONS "-g component -c component/power.train -t E --benchmark -z 2")
  brlcad_regression_test(regress-rtwiz_m35_E "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard)
  distclean(${LOG_FILE} ${TARGET_IMAGE})

  set(INPUT_FILE "${DATA_DIR}/db/m35.g")
  set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_F_ref.pix")
  set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_F.pix")
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_F.log")
  set(OPTIONS "-l component -g component -c component/power.train -t F --benchmark -z 2")
  brlcad_regression_test(regress-rtwiz_m35_F "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard)
  distclean(${LOG_FILE} ${TARGET_IMAGE})

  set(INPUT_FILE "${DATA_DIR}/db/m35.g")
  set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_edge_only_ref.pix")
  set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only.pix")
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only.log")
  set(OPTIONS "-l component --benchmark -z 2")
  brlcad_regression_test(regress-rtwiz_m35_edge_only "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard)
  distclean(${LOG_FILE} ${TARGET_IMAGE})

  set(INPUT_FILE "${DATA_DIR}/db/m35.g")
  set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_edge_only_color_ref.pix")
  set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_color.pix")
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_edge_only_color.log")
  set(OPTIONS "-l component --benchmark -z 2 --line-color 100/200/255 -C 30/30/30")
  brlcad_regression_test(regress-rtwiz_m35_edge_only_color "rtwizard;pixcmp;m35.g" TEST_SCRIPT "${TSCRIPT}" EXEC rtwizard)
  distclean(${LOG_FILE} ${TARGET_IMAGE})
endif(TARGET rtwizard)

cmakefiles(
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
