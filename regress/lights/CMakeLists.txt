if(SH_EXEC AND TARGET asc2g)
  brlcad_add_test(NAME regress-lights COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/lights.sh" ${CMAKE_SOURCE_DIR})
  brlcad_regression_test(regress-lights "rt;asc2g;pixdiff" TEST_DEFINED)
endif(SH_EXEC AND TARGET asc2g)

cmakefiles(
  lights.ref.pix
  lights.sh
)

# list of temporary files
set(
  lights_outfiles
  lights.asc
  lights.diff.pix
  lights.g
  lights.log
  lights.pix
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${lights_outfiles}")
distclean(${lights_outfiles})

cmakefiles(
  CMakeLists.txt
  lights.cmake
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
