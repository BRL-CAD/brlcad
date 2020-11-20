
if (SH_EXEC AND TARGET mged)

  BRLCAD_ADD_TEST(NAME regress-weight COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/weight.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-weight "rtweight;mged" TEST_DEFINED)

endif (SH_EXEC AND TARGET mged)

CMAKEFILES(
  weight.sh
  weight.ref
  weight.test2.ref
  )

# list of temporary files
set(weight_outfiles
  .density
  .density.weight1
  weight.g
  weight.log
  weight.mged
  weight.out
  weight.out_ns
  weight.ref_ns
  weight.test2.g
  weight.test2.mged
  weight.test2.out
  weight.test2.out_ns
  weight.test2.ref_ns
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${weight_outfiles}")
DISTCLEAN(${weight_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
