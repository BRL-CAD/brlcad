
if (SH_EXEC AND TARGET mged)

  BRLCAD_ADD_TEST(NAME regress-bots COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/bots.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-bots "mged;pixdiff;rt" TEST_DEFINED)

endif (SH_EXEC AND TARGET mged)

CMAKEFILES(
  bots.sh
  )

# list of temporary files
set(bots_outfiles
  bots.diff.log
  bots.diff.pix
  bots.g
  bots.lh.pix
  bots.log
  bots.no.pix
  bots.rh.pix
  bots.rl.diff.pix
  bots.rn.diff.pix
  bots.rs.diff.pix
  bots.sph.pix
  bots.sync.pix
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${bots_outfiles}")
DISTCLEAN(${bots_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
