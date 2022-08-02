
if (SH_EXEC AND TARGET mged AND TARGET gqa)

  BRLCAD_ADD_TEST(NAME regress-gqa COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/gqa.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-gqa "gqa;mged" TEST_DEFINED)

endif (SH_EXEC AND TARGET mged AND TARGET gqa)

CMAKEFILES(
  gqa.sh
  )

# list of temporary files
set(gqa_outfiles
  density_table.txt
  gqa.adj_air.plot3
  gqa.exp_air.plot3
  gqa.g
  gqa.gaps.plot3
  gqa.log
  gqa.mged
  gqa.overlaps.plot3
  gqa.volume.plot3
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${gqa_outfiles}")
DISTCLEAN(${gqa_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
