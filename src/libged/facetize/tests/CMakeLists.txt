brlcad_addexec(facetize_worker_protocol_test "worker.cpp;../worker.cpp" "libbu;Threads::Threads" TEST)
brlcad_add_test(NAME facetize_worker_protocol COMMAND facetize_worker_protocol_test)

brlcad_addexec(facetize_validation_test "validation.cpp;../validation.cpp" "libwdb;librt;libbu" TEST)
brlcad_add_test(NAME facetize_validation COMMAND facetize_validation_test)

brlcad_addexec(facetize_recovery_test recovery.cpp "libged;libwdb;librt;libbu" TEST_USESDATA)
if(TARGET facetize_recovery_test)
  add_dependencies(facetize_recovery_test ged_plugins facetize_process)
  set(FACETIZE_RECOVERY_TMP "${CMAKE_CURRENT_BINARY_DIR}/facetize_recovery_tmp")
  brlcad_add_test(
    NAME facetize_recovery
    COMMAND facetize_recovery_test "${FACETIZE_RECOVERY_TMP}"
  )
  set_tests_properties(
    facetize_recovery
    PROPERTIES
      ENVIRONMENT "BU_DIR_CACHE=${FACETIZE_RECOVERY_TMP}/cache"
      TIMEOUT 120
  )
endif(TARGET facetize_recovery_test)

cmakefiles(
  CMakeLists.txt
  recovery.cpp
  validation.cpp
  worker.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
