include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  )

BRLCAD_ADDEXEC(regress_pkg regress_pkg.cpp "libpkg;libbu;${CMAKE_THREAD_LIBS_INIT}" NO_INSTALL)
target_compile_definitions(regress_pkg PUBLIC BRLCADBUILD HAVE_CONFIG_H)
set_target_properties(regress_pkg PROPERTIES FOLDER "BRL-CAD Regression Tests/libpkg")

add_test(NAME regress-pkg
  COMMAND $<TARGET_FILE:regress_pkg>
  )
set_tests_properties(regress-pkg PROPERTIES LABELS "Regression")

if (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-pkg COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_CFG_INTDIR} -R ^regress-pkg --output-on-failure)
else (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-pkg COMMAND ${CMAKE_CTEST_COMMAND} -R ^regress-pkg --output-on-failure)
endif (CMAKE_CONFIGURATION_TYPES)
add_dependencies(regress-pkg regress_pkg)

set_target_properties(regress-pkg PROPERTIES FOLDER "BRL-CAD Regression Tests")
set_target_properties(regress-pkg PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)

CMAKEFILES(
  CMakeLists.txt
  )
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
