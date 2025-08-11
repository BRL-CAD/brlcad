if(SH_EXEC AND TARGET mged AND TARGET gencolor)
  brlcad_add_test(NAME regress-shaders COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/shaders.sh" ${CMAKE_SOURCE_DIR})
  brlcad_regression_test(regress-shaders "rt;mged;pixdiff;gencolor" TEST_DEFINED)
endif(SH_EXEC AND TARGET mged AND TARGET gencolor)

cmakefiles(
  shaders.eagleCAD-512x438.pix
  shaders.ref.pix
  shaders.sh
)

# list of temporary files
set(
  shaders_outfiles
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
distclean(${shaders_outfiles})

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
