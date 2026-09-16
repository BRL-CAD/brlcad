brlcad_addexec(ged_test_bot_dump "bot_dump.cpp" "libged" TEST)
brlcad_add_test(NAME ged_test_bot_dumping COMMAND ged_test_bot_dump "${CMAKE_CURRENT_SOURCE_DIR}")
distclean(${CMAKE_CURRENT_BINARY_DIR}/arb4_norm_out.mtl)

brlcad_addexec(ged_test_bot_split "bot_split.cpp" "libged;libwdb;librt;libbu" TEST)
if(TARGET ged_test_bot_split)
  add_dependencies(ged_test_bot_split ged_plugins)
endif(TARGET ged_test_bot_split)
brlcad_add_test(NAME ged_test_bot_split COMMAND ged_test_bot_split "${CMAKE_CURRENT_BINARY_DIR}/bot_split.g")
if(TARGET ged_test_bot_split)
  set_property(TEST ged_test_bot_split APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif(TARGET ged_test_bot_split)
distclean(${CMAKE_CURRENT_BINARY_DIR}/bot_split.g)

cmakefiles(
  CMakeLists.txt
  bot_split.cpp
  arb4.dxf
  arb4.g
  arb4.obj
  arb4_color.mtl
  arb4_color.obj
  arb4.sat
  arb4.stl
  arb4_binary.stl
  arb4_color.mtl
  arb4_color.obj
  arb4_norm.obj
  arb4_units.stl
  arbs_stl/arb4_bot.stl
  arbs_stl/arb4_bot_01.stl
  arbs_stl/arb4_bot_02.stl
  arbs_stl/arb4_bot_03.stl
  half_subtracts.g
  manifold_fixable.g
  tgc.g
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
