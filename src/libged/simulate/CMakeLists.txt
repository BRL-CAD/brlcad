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
endif(BRLCAD_ENABLE_BULLET)

if(BRLCAD_ENABLE_BULLET AND BULLET_LIBRARIES)
  add_definitions(-DHAVE_BULLET=1)

  # If bullet was built with USE_DOUBLE_PRECISION, we need this (see
  # https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=9670 for more discussion.)
  # System Bullet libraries typically use single precision, so only enable double
  # precision when NOT using system libraries.
  if(NOT BRLCAD_BUNDLED_LIBS STREQUAL "SYSTEM")
    add_definitions(-DBT_USE_DOUBLE_PRECISION=1)
  endif()
endif(BRLCAD_ENABLE_BULLET AND BULLET_LIBRARIES)

set(
  bullet_srcs
  ged_command.cpp
  rt_collision_algorithm.cpp
  rt_collision_shape.cpp
  rt_debug_draw.cpp
  rt_instance.cpp
  rt_motion_state.cpp
  rt_roi_collision_shape.cpp
  simulation.cpp
  utility.cpp
)

set(SIM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/simulate)
set(SIM_SYSTEM_INCLUDE_DIRS ${BULLET_INCLUDE_DIR})
set(SIM_LIBS libged libbu libbv ${BULLET_LIBRARIES})

ged_plugin_library(ged-simulate
  ${bullet_srcs}
  INCLUDES ${SIM_INCLUDE_DIRS}
  SYSTEM_INCLUDES ${SIM_SYSTEM_INCLUDE_DIRS}
  LIBS ${SIM_LIBS}
  COMPILE_OPTIONS -Wno-nonnull-compare
  PLUGIN_ONLY
)

cmakefiles(
  CMakeLists.txt
  ${bullet_srcs}
  SIMULATION_VERIFICATION.md
  TESTING_SUMMARY.md
  rt_collision_algorithm.hpp
  rt_collision_shape.hpp
  rt_debug_draw.hpp
  rt_instance.hpp
  rt_motion_state.hpp
  rt_roi_collision_shape.hpp
  simulation.hpp
  tests/CMakeLists.txt
  tests/simulate_test.cpp
  tests/verify_simulation.c
  utility.hpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
