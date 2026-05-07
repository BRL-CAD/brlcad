if(BRLCAD_ENABLE_BULLET)
  # There is a peculiarity about using Bullet libraries - by design, they are
  # intended to be used ONLY as static libraries and are not reliably viable
  # for use in a shared configuration:
  #
  # https://stackoverflow.com/a/37019528
  #
  # Additionally, when compiling Bullet to produce static libs on Ubuntu,
  # it proved necessary to add -DCMAKE_POSITION_INDEPENDENT_CODE=ON to
  # avoid linking errors.
  #
  # The need for static libs complicates searching for system installs, since
  # Linux package managers do provide shared versions (at least on Ubuntu,
  # there doesn't even seem to be an option for static libraries provided by
  # the OS?)
  #
  # To avoid finding versions that won't work for our build, we use a variation
  # on this trick:  https://www.scivision.dev/cmake-force-link-static-libs/
  # However, when using system libraries (BRLCAD_BUNDLED_LIBS=SYSTEM), we allow
  # shared libraries to enable faster builds.
  if(NOT BRLCAD_BUNDLED_LIBS STREQUAL "SYSTEM")
    set(CFLS_TMP ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif()
  find_package(Bullet REQUIRED)
  if(NOT BRLCAD_BUNDLED_LIBS STREQUAL "SYSTEM")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${CFLS_TMP})
  endif()

  set(SIM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/simulate)

  # If bullet was built with USE_DOUBLE_PRECISION, we need this (see
  # https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=9670 for more discussion.)
  # System Bullet libraries typically use single precision, so only enable double
  # precision when NOT using system libraries.
  if(NOT BRLCAD_BUNDLED_LIBS STREQUAL "SYSTEM")
    add_definitions(-DBT_USE_DOUBLE_PRECISION=1)
  endif()

  # Not ready to turn this on by default yet.
  #brlcad_addexec(simulate_test "simulate_test.cpp" "libged;${BULLET_LIBRARIES}" TEST)
  #target_include_directories(simulate_test BEFORE PRIVATE ${SIM_INCLUDE_DIRS})
  #target_include_directories(simulate_test SYSTEM PRIVATE ${BULLET_INCLUDE_DIR})
  #target_compile_options(simulate_test PRIVATE -Wno-nonnull-compare)
  #brlcad_add_test(NAME simulate_test COMMAND simulate_test)

  # verify_simulation: standalone tool for pre/post-simulation checks on an
  # actual on-disk .g database (m35 truck + terra terrain scenario).
  # Not run as an automated CTest because it requires a built simulation
  # database; it is provided as an offline verification utility.
  #brlcad_addexec(verify_simulation "verify_simulation.c" "libged;librt;libbu" NO_INSTALL)

  # M35 truck simulation integration test.
  # Requires: m35.g and terra.g built from their .asc sources (targets m35_g
  # and terra_g), mged, and the ged-simulate plugin (Bullet).
  # The test builds the combined simulation database via setup_sim.tcl, runs
  # a single 0.5-second simulation step, and checks that the truck has begun
  # to fall (Z coordinate of truck.sim has decreased).
  #if(TARGET m35_g AND TARGET terra_g)
  #  set(M35_SETUP_TCL "${BRLCAD_SOURCE_DIR}/misc/simulate/setup_sim.tcl")
  #  set(M35_SIM_DB    "${CMAKE_CURRENT_BINARY_DIR}/m35_sim_test.g")
  #
  #  # Step 1: build the combined database
  #  add_custom_command(
  #    OUTPUT  "${M35_SIM_DB}"
  #    COMMAND ${CMAKE_COMMAND} -E remove -f "${M35_SIM_DB}"
  #    COMMAND mged -c "${M35_SIM_DB}" < "${M35_SETUP_TCL}"
  #    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${DATA_DIR}/db"
  #    DEPENDS mged m35_g terra_g "${M35_SETUP_TCL}"
  #    COMMENT "Building M35 truck simulation database"
  #  )
  #  add_custom_target(m35_sim_db DEPENDS "${M35_SIM_DB}")

  #   Step 2: run one simulation step, then verify  (not ready to turn
  #   this on by default yet)
  #  brlcad_add_test(
  #    NAME    simulate_m35
  #    COMMAND "${CMAKE_BINARY_DIR}/${BIN_DIR}/mged" -c "${M35_SIM_DB}"
  #            "simulate --steps 1 scene.c 0.5"
  #  )
  #  set_tests_properties(simulate_m35 PROPERTIES
  #    DEPENDS m35_sim_db
  #    PASS_REGULAR_EXPRESSION "."
  #  )
  #endif()

endif(BRLCAD_ENABLE_BULLET)

cmakefiles(
  CMakeLists.txt
  simulate_test.cpp
  verify_simulation.c
  example/README.md
  example/render_frames.py
  example/render_sample.png
  example/run_sim.sh
  example/setup_sim.tcl
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
