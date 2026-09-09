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
