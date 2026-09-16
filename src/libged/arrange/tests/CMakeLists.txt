brlcad_addexec(arrange_test_masks
  "arrange_test_masks.cpp;../nest_candidates.cpp;../nest_raster.cpp;../nest_search.cpp"
  "libanalyze;librt;libbu"
  TEST
)
brlcad_add_test(NAME arrange_test_masks COMMAND arrange_test_masks)

brlcad_addexec(arrange_test_command arrange_test_command.cpp
  "libged;libwdb;librt;libbu"
  TEST
)
if(TARGET arrange_test_command)
  add_dependencies(arrange_test_command ged_plugins)
endif(TARGET arrange_test_command)
brlcad_add_test(NAME arrange_test_command COMMAND arrange_test_command)
if(TARGET arrange_test_command)
  set_property(TEST arrange_test_command APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif(TARGET arrange_test_command)

cmakefiles(
  CMakeLists.txt
  README.md
  arrange_examples.mged
  run_m35_benchmark.py
  arrange_test_command.cpp
  arrange_test_masks.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
