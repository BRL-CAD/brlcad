
if (SH_EXEC AND TARGET mged)

  if (NOT WIN32)
    BRLCAD_ADD_TEST(NAME regress-red COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/red.sh" ${CMAKE_SOURCE_DIR})
    BRLCAD_REGRESSION_TEST(regress-red mged TEST_DEFINED)
  else (NOT WIN32)
    message(WARNING "regress-red does not currently pass on Windows, disabling")
  endif (NOT WIN32)

endif (SH_EXEC AND TARGET mged)

CMAKEFILES(
  red.sh
  )

# list of temporary files
set(red_outfiles
  red.air.empty.out
  red.air.empty.out.new
  red.air.safe.out
  red.air.safe.out.new
  red.air.safe.out.test
  red.air.unsafe.out
  red.air.unsafe.out.new
  red.air.unsafe.out.test
  red.color.delim.out
  red.color.delim.out.new
  red.color.delim.out.test
  red.color.empty.out
  red.color.empty.out.new
  red.color.safe.out
  red.color.safe.out.new
  red.color.safe.out.test
  red.color.unsafe.out
  red.color.unsafe.out.new
  red.edit.sh
  red.g
  red.hps.r.out
  red.log
  red.los.empty.out
  red.los.empty.out.new
  red.los.safe.out
  red.los.safe.out.new
  red.los.safe.out.test
  red.los.unsafe.out
  red.los.unsafe.out.new
  red.los.unsafe.out.test
  red.material_id.empty.out
  red.material_id.empty.out.new
  red.material_id.safe.out
  red.material_id.safe.out.new
  red.material_id.safe.out.test
  red.material_id.unsafe.out
  red.material_id.unsafe.out.new
  red.material_id.unsafe.out.test
  red.matrix.noop.out
  red.matrix.noop.out.new
  red.name.empty.out
  red.name.empty.out.new
  red.name.emptyedit.out
  red.name.emptyedit.out.new
  red.name.safe.out
  red.name.safe.out.new
  red.name.safe.out.test
  red.name.unsafe.out
  red.name.unsafe.out.new
  red.name.unsafe.out.test
  red.region.edit-comb.out
  red.region.edit-comb.out.new
  red.region.edit-comb.out.test
  red.region.edit-comb.out.test2
  red.region.empty.out
  red.region.empty.out.new
  red.region.no.out
  red.region.no.out.new
  red.region.one.out
  red.region.one.out.new
  red.region.yes.out
  red.region.yes.out.new
  red.region.zero.out
  red.region.zero.out.new
  red.region_id.empty.out
  red.region_id.empty.out.new
  red.region_id.safe.out
  red.region_id.safe.out.new
  red.region_id.safe.out.test
  red.region_id.unsafe.out
  red.region_id.unsafe.out.new
  red.region_id.unsafe.out.test
  red.shader.empty.out
  red.shader.empty.out.new
  red.shader.safe.out
  red.shader.safe.out.new
  red.shader.safe.out.test
  red.shader.unsafe.out
  red.shader.unsafe.out.new
  red.shader.unsafe.out.test
  red.sph.r.out
  red.sph_rot.c.out
  red.tree.empty.out
  red.tree.empty.out.new
  red.tree.safe.out
  red.tree.safe.out.new
  red.tree.safe.out.test
  red.tree.unsafe.out
  red.tree.unsafe.out.new
  red.virgin.out
  red.virgin.out.new
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${red_outfiles}")
DISTCLEAN(${red_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
