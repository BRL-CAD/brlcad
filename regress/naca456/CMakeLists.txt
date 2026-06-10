if(SH_EXEC AND TARGET naca456 AND TARGET mged)
  brlcad_add_test(
    NAME regress-naca456
    COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/naca456.sh" ${CMAKE_SOURCE_DIR} "$<TARGET_FILE:naca456>" "$<TARGET_FILE:mged>"
    )
  brlcad_regression_test(regress-naca456 "naca456;mged" TEST_DEFINED STAND_ALONE)
endif(SH_EXEC AND TARGET naca456 AND TARGET mged)

cmakefiles(
  CMakeLists.txt
  naca456.sh
  pdas_sections.tsv
)

set(
  naca456_outfiles
  naca456.log
  naca456-bot.g
  naca456-five-bot.g
  naca456-sharp-brep.g
  naca456-five-reflex-brep.g
  naca456-six-bot.g
  naca456-sixa-brep.g
  naca456-append.g
  naca456-demo.g
  naca456-sections.tsv
  naca456-section-2412.tsv
  naca456-section-23012.tsv
  naca456-section-23112.tsv
  naca456-section-63-206.tsv
  naca456-section-63a210.tsv
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${naca456_outfiles}")
distclean(${naca456_outfiles})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
