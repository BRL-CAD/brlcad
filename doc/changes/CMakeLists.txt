set(CHANGES_RULES_FILE "${CMAKE_CURRENT_SOURCE_DIR}/minimally-impacting.rules")
set(CHANGES_BUILD_DIR "${CMAKE_BINARY_DIR}/doc/changes")
set(CHANGES_GENERATED_ADOC "${CHANGES_BUILD_DIR}/minimally-impacting.adoc")
set(CHANGES_GENERATED_SCRIPT "${CHANGES_BUILD_DIR}/changes_apply_rules.sh")

add_custom_command(
  OUTPUT "${CHANGES_GENERATED_ADOC}" "${CHANGES_GENERATED_SCRIPT}"
  COMMAND
    "${CMAKE_COMMAND}"
    -DCHANGES_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    -DCHANGES_BINARY_DIR="${CMAKE_BINARY_DIR}"
    -DCHANGES_RULES_FILE="${CHANGES_RULES_FILE}"
    -DCHANGES_APPLY_TEMPLATE="${CMAKE_CURRENT_SOURCE_DIR}/changes_apply_rules.sh.in"
    -DCHANGES_GENERATED_ADOC="${CHANGES_GENERATED_ADOC}"
    -DCHANGES_GENERATED_SCRIPT="${CHANGES_GENERATED_SCRIPT}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/changes_generate_rules.cmake"
  DEPENDS
    "${CHANGES_RULES_FILE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/changes_generate_rules.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/changes_apply_rules.sh.in"
  COMMENT "Generating BRL-CAD change-rule documentation and updater"
  VERBATIM
)

add_custom_target(changes-generate-rules DEPENDS "${CHANGES_GENERATED_ADOC}" "${CHANGES_GENERATED_SCRIPT}")
set_target_properties(changes-generate-rules PROPERTIES FOLDER "DocBuild/Changes")

add_custom_target(
  changes-check-rules
  COMMAND
    "${CMAKE_COMMAND}"
    -DCHANGES_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    -DCHANGES_BINARY_DIR="${CMAKE_BINARY_DIR}"
    -DCHANGES_RULES_FILE="${CHANGES_RULES_FILE}"
    -DCHANGES_APPLY_TEMPLATE="${CMAKE_CURRENT_SOURCE_DIR}/changes_apply_rules.sh.in"
    -DCHANGES_GENERATED_ADOC="${CHANGES_GENERATED_ADOC}"
    -DCHANGES_GENERATED_SCRIPT="${CHANGES_GENERATED_SCRIPT}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/changes_check_rules.cmake"
  DEPENDS
    "${CHANGES_RULES_FILE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/changes_check_rules.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/changes_generate_rules.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/changes_apply_rules.sh.in"
)
set_target_properties(changes-check-rules PROPERTIES FOLDER "DocBuild/Changes")

add_custom_target(
  changes-api-snapshot
  COMMAND
    "${CMAKE_COMMAND}"
    -DCHANGES_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    -DCHANGES_API_BASELINE="${CMAKE_CURRENT_SOURCE_DIR}/changes_api_baseline.adoc"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/changes_api_snapshot.cmake"
)
set_target_properties(changes-api-snapshot PROPERTIES FOLDER "DocBuild/Changes")

add_custom_target(
  changes-api-check
  COMMAND
    "${CMAKE_COMMAND}"
    -DCHANGES_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    -DCHANGES_BINARY_DIR="${CMAKE_BINARY_DIR}"
    -DCHANGES_API_BASELINE="${CMAKE_CURRENT_SOURCE_DIR}/changes_api_baseline.adoc"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/changes_api_check.cmake"
)
set_target_properties(changes-api-check PROPERTIES FOLDER "DocBuild/Changes")

if(SH_EXEC)
  brlcad_add_test(
    NAME regress-changes-rules
    COMMAND
      ${SH_EXEC}
      "${CMAKE_CURRENT_SOURCE_DIR}/changes_rules_test.sh"
      "${CMAKE_COMMAND}"
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}"
  )
  brlcad_regression_test(regress-changes-rules "" STAND_ALONE TEST_DEFINED)
endif(SH_EXEC)

cmakefiles(
  CMakeLists.txt
  README.adoc
  changes_api_baseline.adoc
  changes_api_check.cmake
  changes_api_snapshot.cmake
  changes_apply_rules.sh.in
  changes_check_rules.cmake
  changes_generate_rules.cmake
  changes_rules_test.sh
  deprecated.adoc
  minimally-impacting.rules
  obsoleted.adoc
)

distclean("${CHANGES_BUILD_DIR}/minimally-impacting.adoc")
distclean("${CHANGES_BUILD_DIR}/changes_apply_rules.sh")
distclean("${CHANGES_BUILD_DIR}/changes_api_current.adoc")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
