brlcad_addexec(test_gcv_gson test_gcv_gson.cpp "libgcv;libwdb;librt;libbu" NO_INSTALL)
add_dependencies(test_gcv_gson gcv-gson)
brlcad_add_test(NAME gcv_gson_articulation_roundtrip COMMAND test_gcv_gson "${CMAKE_CURRENT_BINARY_DIR}/articulation.gson")

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
  "${CMAKE_CURRENT_BINARY_DIR}/articulation.gson"
)
distclean("${CMAKE_CURRENT_BINARY_DIR}/articulation.gson")

cmakefiles(CMakeLists.txt test_gcv_gson.cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
