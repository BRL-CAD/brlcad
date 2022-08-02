
if (SH_EXEC AND TARGET asc2g)

  BRLCAD_ADD_TEST(NAME regress-lights COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/lights.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-lights "rt;asc2g;pixdiff" TEST_DEFINED)

endif (SH_EXEC AND TARGET asc2g)

CMAKEFILES(
  lights.ref.pix
  lights.sh
  )

# list of temporary files
set(lights_outfiles
  lights.asc
  lights.diff.pix
  lights.g
  lights.log
  lights.pix
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${lights_outfiles}")
DISTCLEAN(${lights_outfiles})

CMAKEFILES(
  CMakeLists.txt
  lights.cmake
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
