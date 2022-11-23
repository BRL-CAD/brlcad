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
target_link_libraries(ged-simulate libged libbu)
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
