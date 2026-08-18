brlcad_add_test(
  NAME util_bwcrop_single_axis
  COMMAND
    "${CMAKE_COMMAND}" "-DBWCROP=$<TARGET_FILE:bwcrop>" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_bwcrop_single_axis.cmake"
)

brlcad_add_test(
  NAME util_pixdiff_bytes
  COMMAND
    "${CMAKE_COMMAND}" "-DPIXDIFF=$<TARGET_FILE:pixdiff>" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_pixdiff_bytes.cmake"
)

brlcad_add_test(
  NAME util_pixembed_edge_replication
  COMMAND
    "${CMAKE_COMMAND}" "-DPIXEMBED=$<TARGET_FILE:pixembed>" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_pixembed_edge_replication.cmake"
)

brlcad_add_test(
  NAME util_bwstat_statistics
  COMMAND
    "${CMAKE_COMMAND}" "-DBWSTAT=$<TARGET_FILE:bwstat>" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_bwstat_statistics.cmake"
)

cmakefiles(
  CMakeLists.txt
  test_bwcrop_single_axis.cmake
  test_bwstat_statistics.cmake
  test_pixdiff_bytes.cmake
  test_pixembed_edge_replication.cmake
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
