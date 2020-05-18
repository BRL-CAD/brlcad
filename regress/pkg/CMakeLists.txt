include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  )

add_executable(regress_pkg_server regress_pkg_server.c)
target_link_libraries(regress_pkg_server libbu libpkg)
set_target_properties(regress_pkg_server PROPERTIES FOLDER "BRL-CAD Regression Tests")

add_executable(regress_pkg_client regress_pkg_client.c)
target_link_libraries(regress_pkg_client libbu libpkg)
set_target_properties(regress_pkg_client PROPERTIES FOLDER "BRL-CAD Regression Tests")

# CMake executes programs in series, and since the client and server need
# to be running in parallel for this to work we have to do something else.
# We use a single C++ utility program to handle the launch and report the
# results.
add_executable(regress_pkg regress_pkg.cpp)
target_link_libraries(regress_pkg libbu ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(regress_pkg PROPERTIES FOLDER "BRL-CAD Regression Tests")
add_dependencies(regress_pkg regress_pkg_server regress_pkg_client)

add_test(NAME regress-pkg
  COMMAND $<TARGET_FILE:regress_pkg> $<TARGET_FILE:regress_pkg_server> $<TARGET_FILE:regress_pkg_client>
  )
set_tests_properties(regress-pkg PROPERTIES LABELS "Regression")

if (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-pkg COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_CFG_INTDIR} -R ^regress-pkg --output-on-failure)
else (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-pkg COMMAND ${CMAKE_CTEST_COMMAND} -R ^regress-pkg --output-on-failure)
endif (CMAKE_CONFIGURATION_TYPES)
add_dependencies(regress-pkg libbu libpkg)
set_target_properties(regress-pkg PROPERTIES FOLDER "BRL-CAD Regression Tests")
set_target_properties(regress-pkg PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
add_dependencies(regress regress-pkg)
add_dependencies(check regress-pkg)

CMAKEFILES(
  CMakeLists.txt
  regress_pkg.cpp
  regress_pkg_server.c
  regress_pkg_client.c
  regress_pkg_protocol.h
  )
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
