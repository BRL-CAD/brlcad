if(SH_EXEC AND TARGET mged AND TARGET gqa)
  brlcad_add_test(NAME regress-analyze_parity
    COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/analyze_parity.sh" ${CMAKE_SOURCE_DIR}
  )
  brlcad_regression_test(regress-analyze_parity "gqa;mged" TEST_DEFINED)
endif(SH_EXEC AND TARGET mged AND TARGET gqa)

# Temporary output files
set(
  analyze_parity_outfiles
  analyze_parity.log
  parity.g
  parity.mged
)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${analyze_parity_outfiles}")
distclean(${analyze_parity_outfiles})

cmakefiles(
  CMakeLists.txt
  analyze_parity.sh
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
