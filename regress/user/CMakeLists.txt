if(TARGET libged AND BUILD_TESTING)
  # This program is intended to test the usability of BRL-CAD's libraries without
  # the internal compile-time APIs - i.e., to simulate what a BRL-CAD client code
  # might experience when trying to use the BRL-CAD libraries.

  # Due to the nature of this test, it should not see any of the defines used by
  # BRL-CAD to enable compile-time API.  We first clear any inherited include
  # directories or compile definitions as a precaution:
  set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS "")
  set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "")

  # NOTE - for the moment we are not overriding the compile flags (optimization,
  # warnings, etc.) from the parent build, although eventually we should probably
  # look into doing so...  We want the strictness of those checks to apply to
  # this code as well, so long as they don't also contain anything that ties this
  # code to any internal, compile time behaviors or features.

  # We deliberately do not use any of the BRL-CAD wrapper functions such as
  # BRLCAD_ADDEXEC in order to avoid their automatic addition of compile
  # definitions or other magic specific to BRL-CAD's internal build.
  add_executable(cad_user cad_user.c)
  target_link_libraries(cad_user libged)
  add_dependencies(cad_user managed_files)

  # Include only the folder with the build-output headers (this will be a
  # superset of what will be installed, but is the best available option without
  # performing an actual install step.)
  target_include_directories(cad_user BEFORE PUBLIC "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/brlcad")

  set_target_properties(cad_user PROPERTIES FOLDER "BRL-CAD Regression Tests/user")

  if(TARGET moss.g)
    set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-user.log")
    brlcad_regression_test(regress-user "cad_user;moss.g" EXEC cad_user)
    distclean(${LOG_FILE})
  endif(TARGET moss.g)
endif(TARGET libged AND BUILD_TESTING)

if(BRLCAD_ENABLE_BRLCAD_LIBRARY AND TARGET libbrlcad AND BUILD_TESTING)
  # Simulate a 3rd party application using the aggregate library: public
  # headers only, no BRL-CAD internal build helpers, and link to libbrlcad only.
  set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS "")
  set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "")

  add_executable(cad_user_aggregate cad_user_aggregate.c)
  target_link_libraries(cad_user_aggregate libbrlcad)
  add_dependencies(cad_user_aggregate managed_files)
  if(TARGET gcv_plugins)
    add_dependencies(cad_user_aggregate gcv_plugins)
  endif(TARGET gcv_plugins)

  target_include_directories(cad_user_aggregate BEFORE PUBLIC "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/brlcad")

  set_target_properties(cad_user_aggregate PROPERTIES FOLDER "BRL-CAD Regression Tests/user")

  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-user-aggregate.log")
  brlcad_regression_test(
    regress-user-aggregate
    "cad_user_aggregate;libbrlcad"
    EXEC cad_user_aggregate
    TEST_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-user-aggregate.cmake.in"
  )
  distclean(${LOG_FILE})
endif(BRLCAD_ENABLE_BRLCAD_LIBRARY AND TARGET libbrlcad AND BUILD_TESTING)

cmakefiles(
  CMakeLists.txt
  cad_user.c
  cad_user_aggregate.c
  regress-user-aggregate.cmake.in
  regress-user.cmake.in
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
