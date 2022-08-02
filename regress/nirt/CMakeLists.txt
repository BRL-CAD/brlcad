
if (SH_EXEC AND TARGET mged)

  BRLCAD_ADD_TEST(NAME regress-nirt COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/nirt.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-nirt "mged;nirt" TEST_DEFINED)

endif (SH_EXEC AND TARGET mged)

CMAKEFILES(
  nirt.sh
  ovlps.g
  )

# list of temporary files
set(nirt_outfiles
  nirt.g
  nirt.log
  nirt.mged
  nirt.out
  nirt.ref
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${nirt_outfiles}")
DISTCLEAN(${nirt_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
