add_executable(creo_snapshot_core_test test_snapshot_core.cpp)
target_include_directories(creo_snapshot_core_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(creo_snapshot_core_test PRIVATE creo-brl-core)
set_target_properties(
creo_snapshot_core_test
PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CREO_BUNDLE_BIN_DIR}"
)

add_test(
    NAME creo_snapshot_core
    COMMAND "$<TARGET_FILE:creo_snapshot_core_test>"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

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
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        "${gap_case}"
  )
  set_tests_properties("creo_chunk5_gap_${gap_name}" PROPERTIES LABELS "chunk5-gap")
endforeach()

add_executable(creo_core_abi_test test_core_abi.cpp)
target_include_directories(creo_core_abi_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_dependencies(creo_core_abi_test creo-brl-core)
set_target_properties(
creo_core_abi_test
PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CREO_BUNDLE_BIN_DIR}"
)

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
set_target_properties(
    creo_profile_options_test
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CREO_BUNDLE_BIN_DIR}"
)

add_test(
    NAME creo_profile_options
    COMMAND "$<TARGET_FILE:creo_profile_options_test>" "${CMAKE_CURRENT_BINARY_DIR}"
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
add_dependencies(creo_capture_policy_test creo-brl-core)
set_target_properties(
    creo_capture_policy_test
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CREO_BUNDLE_BIN_DIR}"
)

add_test(
    NAME creo_capture_policy
    COMMAND "$<TARGET_FILE:creo_capture_policy_test>"
)

add_test(
    NAME creo_component_rotated_mixed_units
    COMMAND "$<TARGET_FILE:creo_capture_policy_test>" rotated-mixed-units
)

foreach(mesh_case IN ITEMS
    toolkit-derived-units
    capture-failure-classification
    mesh-near-vertices
    mesh-collapsed-facet
)
  string(REPLACE "-" "_" mesh_name "${mesh_case}")
  add_test(
      NAME "creo_chunk5_gap_${mesh_name}"
      COMMAND "$<TARGET_FILE:creo_capture_policy_test>" "${mesh_case}"
  )
  set_tests_properties("creo_chunk5_gap_${mesh_name}" PROPERTIES LABELS "chunk5-gap")
endforeach()
