
if (SH_EXEC AND TARGET mged AND TARGET gencolor)

  BRLCAD_ADD_TEST(NAME regress-shaders COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/shaders.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-shaders "rt;mged;pixdiff;gencolor" TEST_DEFINED)

endif (SH_EXEC AND TARGET mged AND TARGET gencolor)

CMAKEFILES(
  shaders.eagleCAD-512x438.pix
  shaders.ref.pix
  shaders.sh
  )

# list of temporary files
set(shaders_outfiles
  shaders.ebm.bw
  shaders.ell_2.prj
  shaders.g
  shaders.half.prj
  shaders.log
  shaders.mged
  shaders.rt
  shaders.rt.diff.pix
  shaders.rt.log
  shaders.rt.pix
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${shaders_outfiles}")
DISTCLEAN(${shaders_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
