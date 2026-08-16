brlcad_add_test(
  NAME util_pixdiff_bytes
  COMMAND
    ${CMAKE_COMMAND} "-DPIXDIFF=$<TARGET_FILE:pixdiff>" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/pixdiff_test.cmake"
)

brlcad_add_test(
  NAME util_bwcrop_pixembed
  COMMAND
    ${CMAKE_COMMAND} "-DBWCROP=$<TARGET_FILE:bwcrop>" "-DPIXEMBED=$<TARGET_FILE:pixembed>"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/crop_embed_test.cmake"
)

cmakefiles(
  CMakeLists.txt
  crop_embed_test.cmake
  pixdiff_test.cmake
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
