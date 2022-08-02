
if (SH_EXEC AND TARGET mged AND TARGET asc2g)

  if (NOT WIN32)
    BRLCAD_ADD_TEST(NAME regress-iges COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/iges.sh" ${CMAKE_SOURCE_DIR})
    BRLCAD_REGRESSION_TEST(regress-iges "iges-g;g-iges;mged;asc2g" TEST_DEFINED)
  else (NOT WIN32)
    message(WARNING "iges.sh doesn not currently pass on Windows - disabling")
  endif (NOT WIN32)

endif (SH_EXEC AND TARGET mged AND TARGET asc2g)

CMAKEFILES(
  m35.asc.gz
  iges.sh
  )

# list of temporary files
set(iges_outfiles
  iges.export.iges
  iges.export.stdout.iges
  iges.g
  iges.import.export.iges
  iges.import.g
  iges.import2.export.iges
  iges.import2.g
  iges.import3.export.iges
  iges.import3.g
  iges.log
  iges.m35.asc
  iges.m35.g
  iges.m35.r516.iges
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${iges_outfiles}")
DISTCLEAN(${iges_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
