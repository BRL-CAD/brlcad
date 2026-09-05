brlcad_addexec(facetize_worker_protocol_test "worker.cpp;../worker.cpp" libbu TEST)
brlcad_add_test(NAME facetize_worker_protocol COMMAND facetize_worker_protocol_test)

brlcad_addexec(facetize_validation_test "validation.cpp;../validation.cpp" "libwdb;librt;libbu" TEST)
brlcad_add_test(NAME facetize_validation COMMAND facetize_validation_test)

brlcad_addexec(facetize_reuse_test "reuse.cpp;../reuse.cpp" "libged;libwdb;librt;libbu" TEST)
if(TARGET facetize_reuse_test)
  add_dependencies(facetize_reuse_test ged_plugins facetize_process)
endif(TARGET facetize_reuse_test)
set(FACETIZE_REUSE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/facetize_reuse_source.g")
set(FACETIZE_REUSE_WORK "${CMAKE_CURRENT_BINARY_DIR}/facetize_reuse_work.g")
brlcad_add_test(NAME facetize_reuse COMMAND facetize_reuse_test
  "${FACETIZE_REUSE_SOURCE}" "${FACETIZE_REUSE_WORK}")
set_tests_properties(
  facetize_reuse
  PROPERTIES ENVIRONMENT
    "BU_DIR_CACHE=${CMAKE_CURRENT_BINARY_DIR}/facetize_reuse_cache"
)
distclean("${FACETIZE_REUSE_SOURCE}" "${FACETIZE_REUSE_WORK}")

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
  reuse.cpp
  validation.cpp
  worker.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
