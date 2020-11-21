
if (SH_EXEC AND TARGET mged AND TARGET asc2g)

  BRLCAD_ADD_TEST(NAME regress-moss COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/moss.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-moss "rt;asc2g;pixdiff;pix-png;png-pix" TEST_DEFINED)

endif (SH_EXEC AND TARGET mged AND TARGET asc2g)

CMAKEFILES(
  moss.ref.pix
  moss.sh
  )

# list of temporary files
set(moss_outfiles
  moss.asc
  moss.g
  moss.log
  moss.pix
  moss.pix.diff
  moss.pix.png
  moss.pix.png.pix
  moss.roundtrip.diff
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${moss_outfiles}")
DISTCLEAN(${moss_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
