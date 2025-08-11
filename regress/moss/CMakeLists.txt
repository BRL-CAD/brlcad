if(SH_EXEC AND TARGET mged AND TARGET asc2g)
  brlcad_add_test(NAME regress-moss COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/moss.sh" ${CMAKE_SOURCE_DIR})
  brlcad_regression_test(regress-moss "rt;asc2g;pixdiff;pix-png;png-pix" TEST_DEFINED)
endif(SH_EXEC AND TARGET mged AND TARGET asc2g)

cmakefiles(
  moss.ref.pix
  moss.sh
)

# list of temporary files
set(
  moss_outfiles
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
distclean(${moss_outfiles})

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
