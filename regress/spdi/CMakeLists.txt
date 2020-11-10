
if (SH_EXEC AND TARGET mged)

  BRLCAD_ADD_TEST(NAME regress-spdi COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/spdi.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-spdi "rt;mged;pixdiff" TEST_DEFINED)

endif (SH_EXEC AND TARGET mged)

CMAKEFILES(
  spdi.ref.pix
  spdi.sh
  )

# list of temporary files
set(spdi_outfiles
  spdi.diff.pix
  spdi.g
  spdi.log
  spdi.mged
  spdi.pix
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${spdi_outfiles}")
DISTCLEAN(${spdi_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
