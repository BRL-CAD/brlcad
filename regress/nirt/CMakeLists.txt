if(SH_EXEC AND TARGET mged)
  brlcad_add_test(NAME regress-nirt COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/nirt.sh" ${CMAKE_SOURCE_DIR})
  brlcad_regression_test(regress-nirt "mged;nirt" TEST_DEFINED)
endif(SH_EXEC AND TARGET mged)

cmakefiles(
  nirt.sh
  ovlps.g
)

# list of temporary files
set(
  nirt_outfiles
  nirt.g
  nirt.log
  nirt.mged
  nirt.out
  nirt.ref
  nirt.out.raw-E
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${nirt_outfiles}")
distclean(${nirt_outfiles})

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
