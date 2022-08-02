
if (SH_EXEC AND TARGET mged AND TARGET asc2g)

  BRLCAD_ADD_TEST(NAME regress-rtedge COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/rtedge.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-rtedge "rt;rtedge;pixdiff;asc2g" TEST_DEFINED)

endif (SH_EXEC AND TARGET mged AND TARGET asc2g)

CMAKEFILES(
  havoc.g.gz
  rtedge.ref.pix.gz
  rtedge.ref2.pix.gz
  rtedge.ref3.pix.gz
  rtedge.ref4.pix.gz
  rtedge.ref5.pix.gz
  rtedge.sh
  )

# list of temporary files
set(rtedge_outfiles
  rtedge.2.pix
  rtedge.3.pix
  rtedge.4.pix
  rtedge.5.pix
  rtedge.diff.pix
  rtedge.diff2.pix
  rtedge.diff3.pix
  rtedge.diff4.pix
  rtedge.diff5.pix
  rtedge.havoc.g
  rtedge.log
  rtedge.pix
  rtedge.ref.pix
  rtedge.ref2.pix
  rtedge.ref3.pix
  rtedge.ref4.pix
  rtedge.ref5.pix
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${rtedge_outfiles}")
DISTCLEAN(${rtedge_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
