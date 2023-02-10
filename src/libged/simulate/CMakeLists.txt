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
  set(CFLS_TMP ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
  find_package(Bullet REQUIRED)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${CFLS_TMP})

endif(BRLCAD_ENABLE_BULLET)

# Now that we have the bullet include dir (if any), do the includes
set(SIM_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/simulate
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${BULLET_INCLUDE_DIR}
  )
BRLCAD_INCLUDE_DIRS(SIM_INCLUDE_DIRS)

if(BRLCAD_ENABLE_BULLET AND BULLET_LIBRARIES)
  add_definitions(-DHAVE_BULLET=1)

  # If bullet was built with USE_DOUBLE_PRECISION, we need this (see
  # https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=9670 for more discussion.)
  # Hopefully we can find a way to detect this correctly at some point...
  add_definitions(-DBT_USE_DOUBLE_PRECISION=1)
endif(BRLCAD_ENABLE_BULLET AND BULLET_LIBRARIES)

set(bullet_srcs
  ged_command.cpp
  rt_collision_algorithm.cpp
  rt_collision_shape.cpp
  rt_debug_draw.cpp
  rt_instance.cpp
  rt_motion_state.cpp
  simulation.cpp
  utility.cpp
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-simulate SHARED ${bullet_srcs})
target_link_libraries(ged-simulate libged libbu ${BULLET_LIBRARIES})
set_property(TARGET ged-simulate APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-simulate ${bullet_srcs})
PLUGIN_SETUP(ged-simulate ged)

CMAKEFILES(
  CMakeLists.txt
  ${bullet_srcs}
  rt_collision_algorithm.hpp
  rt_collision_shape.hpp
  rt_debug_draw.hpp
  rt_instance.hpp
  rt_motion_state.hpp
  simulation.hpp
  tests/CMakeLists.txt
  tests/simulate_test.cpp
  utility.hpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
