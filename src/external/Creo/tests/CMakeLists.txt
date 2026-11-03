set(CREO_TEST_WORK_DIR "${CMAKE_BINARY_DIR}/Tests/$<CONFIG>/work")


function(creo_configure_test_runtime target)
  set_target_properties(
    ${target}
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CREO_TEST_BIN_DIR}"
  )
  add_dependencies(${target} creo-brl-core)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target}>"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CREO_TEST_WORK_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_FILE:creo-brl-core>"
      ${CREO_BRLCAD_RUNTIME_DLLS}
      "$<TARGET_FILE_DIR:${target}>"
    COMMAND_EXPAND_LISTS
  )
endfunction()


add_executable(creo_snapshot_core_test
    test_snapshot_core.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../shared/conversion_snapshot.cpp"
)
target_include_directories(creo_snapshot_core_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(creo_snapshot_core_test PRIVATE
    creo-brl-core
    ${CREO_BRLCAD_LIBS}
)
creo_configure_test_runtime(creo_snapshot_core_test)

add_test(
    NAME creo_snapshot_core
    COMMAND "$<TARGET_FILE:creo_snapshot_core_test>"
    "${CREO_TEST_WORK_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

# Keep repaired and outstanding audit cases under the same label until the
# Chunk 5 adversarial review is closed.
foreach(gap_case IN ITEMS
    name-numeric-only
    name-whitespace
    name-regex
    assembly-rename
)
  string(REPLACE "-" "_" gap_name "${gap_case}")
  add_test(
      NAME "creo_chunk5_gap_${gap_name}"
      COMMAND "$<TARGET_FILE:creo_snapshot_core_test>"
        "${CREO_TEST_WORK_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        "${gap_case}"
  )
  set_tests_properties("creo_chunk5_gap_${gap_name}" PROPERTIES LABELS "chunk5-gap")
endforeach()

add_executable(creo_core_abi_test test_core_abi.cpp)
target_include_directories(creo_core_abi_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(creo_core_abi_test PRIVATE ${CREO_BRLCAD_LIBS})
creo_configure_test_runtime(creo_core_abi_test)

add_test(
    NAME creo_core_abi
    COMMAND "$<TARGET_FILE:creo_core_abi_test>" "$<TARGET_FILE:creo-brl-core>"
)

add_executable(creo_profile_options_test
    test_profile_options.cpp
    ${CREO2BRL_FRONTEND_OPTION_SOURCE_PATHS}
)
target_include_directories(creo_profile_options_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(creo_profile_options_test PRIVATE
    creo-brl-core
    ${CREO_BRLCAD_LIBS}
)
creo_configure_test_runtime(creo_profile_options_test)

add_test(
    NAME creo_profile_options
    COMMAND "$<TARGET_FILE:creo_profile_options_test>"
      "${CREO_TEST_WORK_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/../resources/Creo_to_BRL.g"
)

add_executable(creo_capture_policy_test
    test_capture_policy.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../frontend/model_capture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../frontend/csg_capture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../shared/mesh_normalization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../shared/name_selection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../shim.cpp"
)
target_include_directories(creo_capture_policy_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(creo_capture_policy_test PRIVATE TEST_BUILD BRLCADBUILD HAVE_CONFIG_H)
target_link_libraries(creo_capture_policy_test PRIVATE ${CREO_BRLCAD_LIBS})
creo_configure_test_runtime(creo_capture_policy_test)

add_test(
    NAME creo_capture_policy
    COMMAND "$<TARGET_FILE:creo_capture_policy_test>"
)

add_test(
    NAME creo_component_rotated_mixed_units
    COMMAND "$<TARGET_FILE:creo_capture_policy_test>" rotated-mixed-units
)

# Frontend-side audit cases share the same label as the replay gaps.
foreach(gap_case IN ITEMS
    toolkit-derived-units
    capture-failure-classification
    mesh-near-vertices
    mesh-collapsed-facet
)
  string(REPLACE "-" "_" gap_name "${gap_case}")
  add_test(
      NAME "creo_chunk5_gap_${gap_name}"
      COMMAND "$<TARGET_FILE:creo_capture_policy_test>" "${gap_case}"
  )
  set_tests_properties("creo_chunk5_gap_${gap_name}" PROPERTIES LABELS "chunk5-gap")
endforeach()
