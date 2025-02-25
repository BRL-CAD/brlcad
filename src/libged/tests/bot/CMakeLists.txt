brlcad_addexec(ged_test_bot_dump "bot_dump.cpp" "libged" TEST)

cmakefiles(
  CMakeLists.txt
  arb4.dxf
  arb4.g
  arb4.obj
  arb4.sat
  arb4.stl
  arb4_binary.stl
  arb4_norm.obj
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
