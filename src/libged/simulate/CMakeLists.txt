if(BRLCAD_ENABLE_BULLET)
  set(BULLET_ROOT ${PROJECT_BINARY_DIR})
  brlcad_find_package(Bullet REQUIRED)

  if(BULLET_LIBRARIES)
    add_definitions(-DHAVE_BULLET=1)
    # We build double precision
    if (BULLET_INCLUDE_DIR MATCHES "${PROJECT_BINARY_DIR}/.*")
      add_definitions(-DBT_USE_DOUBLE_PRECISION=1)
    endif()

    get_filename_component(BULLET_INCLUDE_DIR_FULL "${BULLET_INCLUDE_DIR}" ABSOLUTE)

    if (NOT EXISTS "${BULLET_INCLUDE_DIR_FULL}")
      set(BULLET_INCLUDE_DIR "${PROJECT_BINARY_DIR}/${BULLET_INCLUDE_DIR}")
      if (NOT EXISTS "${BULLET_INCLUDE_DIR}")
	message(FATAL_ERROR "Invalid Bullet include directory: ${BULLET_INCLUDE_DIR}")
      endif()
    else()
      set(BULLET_INCLUDE_DIR "${BULLET_INCLUDE_DIR_FULL}")
      message("EXISTS:  BULLET_INCLUDE_DIR: ${BULLET_INCLUDE_DIR}")
    endif()
    set(BULLET_INCLUDE_DIRS "${BULLET_INCLUDE_DIR}")
    if (NOT EXISTS "${BULLET_INCLUDE_DIRS}")
      message(FATAL_ERROR "Invalid Bullet include directory: ${BULLET_INCLUDE_DIRS}")
    endif()
    set(BULLET_INCLUDE_DIRS "${BULLET_INCLUDE_DIRS}")
  endif()
  message("BULLET_LIBRARIES: ${BULLET_LIBRARIES}")
endif()

add_subdirectory(tests)

set(
  bullet_srcs
  ged_command.cpp
  rt_collision_algorithm.cpp
  rt_collision_shape.cpp
  rt_debug_draw.cpp
  rt_instance.cpp
  rt_motion_state.cpp
  rt_roi_collision_shape.cpp
  sim_animate.cpp
  simulation.cpp
  utility.cpp
)

set(SIM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/simulate)
set(SIM_SYSTEM_INCLUDE_DIRS ${BULLET_INCLUDE_DIRS})
set(SIM_LIBS libged libicv libbv libbu ${BULLET_LIBRARIES})

check_cxx_compiler_flag(-Wno-nonnull-compare NO_NONNULL_COMPARE)

ged_plugin_library(ged-simulate
  ${bullet_srcs}
  INCLUDES ${SIM_INCLUDE_DIRS}
  SYSTEM_INCLUDES ${SIM_SYSTEM_INCLUDE_DIRS}
  LIBS ${SIM_LIBS}
  COMPILE_OPTIONS $<$<BOOL:${NO_NONNULL_COMPARE}>:-Wno-nonnull-compare>
  PLUGIN_ONLY
)

cmakefiles(
  CMakeLists.txt
  ${bullet_srcs}
  NOTES
  rt_collision_algorithm.hpp
  rt_collision_shape.hpp
  rt_debug_draw.hpp
  rt_instance.hpp
  rt_motion_state.hpp
  rt_roi_collision_shape.hpp
  sim_animate.hpp
  simulation.hpp
  utility.hpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
