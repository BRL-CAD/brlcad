if(SH_EXEC AND TARGET mkbuilding AND TARGET mged)
  brlcad_add_test(
    NAME regress-building
    COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/building.sh" ${CMAKE_SOURCE_DIR} "$<TARGET_FILE:mkbuilding>" "$<TARGET_FILE:mged>"
    )
  brlcad_regression_test(regress-building "mkbuilding;mged" TEST_DEFINED)
endif(SH_EXEC AND TARGET mkbuilding AND TARGET mged)

cmakefiles(
  CMakeLists.txt
  building.sh
)

set(
  building_outfiles
  building.log
  building-cli.g
  building-effective.json
  building-native.g
  building-osm.g
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${building_outfiles}")
distclean(${building_outfiles})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
