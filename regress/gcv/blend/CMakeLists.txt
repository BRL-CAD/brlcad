brlcad_addexec(blend_fixture blend_fixture.cpp libbu NO_INSTALL)

set(BLEND_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/tetra.blend")
add_custom_command(
  OUTPUT "${BLEND_FIXTURE}"
  COMMAND $<TARGET_FILE:blend_fixture> "${BLEND_FIXTURE}"
  DEPENDS blend_fixture
  VERBATIM
)
add_custom_target(blend_fixture_file DEPENDS "${BLEND_FIXTURE}")
set_target_properties(blend_fixture_file PROPERTIES FOLDER "BRL-CAD Regression Tests/gcv")

gcv_regress_util(
  "blend"
  "tetra.blend"
  DEPS "gcv;gcv-blend;blend_fixture_file"
  EXPECTED_FILE_SIZE "14936"
  EXPECTED_FILE_SIZE_DELTA "128"
)

distclean("${BLEND_FIXTURE}")

cmakefiles(
  CMakeLists.txt
  blend_fixture.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
