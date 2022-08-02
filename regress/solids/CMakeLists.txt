
if (SH_EXEC AND TARGET mged AND TARGET asc2g)

  BRLCAD_ADD_TEST(NAME regress-solids COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/solids.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-solids "rt;asc2g;asc2pix;pixdiff;pix-png;png-pix" TEST_DEFINED)

endif (SH_EXEC AND TARGET mged AND TARGET asc2g)

CMAKEFILES(
  solids.dsp.dat
  solids.mged
  solids.ref.pix
  solids.sh
  solids.simple.mged
  solids.simple.ref.pix
  )

# list of temporary files
set(solids_outfiles
  solids.dsp.pix
  solids.ebm.bw
  solids.g
  solids.log
  solids.pix.diff
  solids.rt
  solids.rt.log
  solids.rt.pix
  solids.simple.g
  solids.simple.pix.diff
  solids.simple.rt
  solids.simple.rt.log
  solids.simple.rt.pix
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${solids_outfiles}")
DISTCLEAN(${solids_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
