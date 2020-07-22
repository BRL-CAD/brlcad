include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

# if GED command stuff is pushed into the actual subdirs, the autoconf vs cmake
# verification (sh/cmakecheck.sh) will fail on dist-hook.  For now, just
# handle it as part of libged
if(BRLCAD_ENABLE_BULLET)
  add_definitions(-DHAVE_BULLET=1)
  set(LIBGED_CMD_INCLUDE_DIRS
    ${LIBGED_CMD_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/simulate
    )
  include_directories(SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/bullet
    ${CMAKE_CURRENT_SOURCE_DIR}/LinearMath
    ${CMAKE_CURRENT_SOURCE_DIR}/BulletCollision
    ${CMAKE_CURRENT_SOURCE_DIR}/BulletDynamics
    ${CMAKE_CURRENT_SOURCE_DIR}/BulletSoftBody
    )
endif(BRLCAD_ENABLE_BULLET)
include(bullet/CMakeLists.txt)

if(0)
add_definitions(-DGED_PLUGIN)
add_library(ged-simulate SHARED simulate.c)
target_link_libraries(ged-simulate libged libbu)
set_property(TARGET ged-simulate APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-simulate simulate.c)
PLUGIN_SETUP(ged-simulate ged)
endif(0)

CMAKEFILES(
  CMakeLists.txt
  ged_command.cpp
  rt_collision_algorithm.cpp
  rt_collision_algorithm.hpp
  rt_collision_shape.cpp
  rt_collision_shape.hpp
  rt_debug_draw.cpp
  rt_debug_draw.hpp
  rt_instance.cpp
  rt_instance.hpp
  rt_motion_state.cpp
  rt_motion_state.hpp
  simulation.cpp
  simulation.hpp
  tests/CMakeLists.txt
  tests/simulate_test.cpp
  utility.cpp
  utility.hpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8


