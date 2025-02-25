brlcad_addexec(ged_test_bot_dump "bot_dump.cpp" "libged" TEST)
brlcad_add_test(NAME ged_test_bot_dumping COMMAND ged_test_bot_dump "${CMAKE_CURRENT_SOURCE_DIR}")

cmakefiles(
  CMakeLists.txt
  arb4.dxf
  arb4.g
  arb4.obj
  arb4.sat
  arb4.stl
  arb4_binary.stl
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
