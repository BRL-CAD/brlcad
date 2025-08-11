if(SH_EXEC AND TARGET mged)
  brlcad_add_test(NAME regress-spdi COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/spdi.sh" ${CMAKE_SOURCE_DIR})
  brlcad_regression_test(regress-spdi "rt;mged;pixdiff" TEST_DEFINED)
endif(SH_EXEC AND TARGET mged)

cmakefiles(
  spdi.ref.pix
  spdi.sh
)

# list of temporary files
set(
  spdi_outfiles
  spdi.diff.pix
  spdi.g
  spdi.log
  spdi.mged
  spdi.pix
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${spdi_outfiles}")
distclean(${spdi_outfiles})

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
