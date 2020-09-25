include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(DEBUG_CMD_CPP "${CMAKE_CURRENT_BINARY_DIR}/debug_cmd.cpp")

add_custom_command(OUTPUT ${DEBUG_CMD_CPP}
  COMMAND debug2c ${CMAKE_BINARY_DIR}/cmakefiles.cmake ${DEBUG_CMD_CPP}
  DEPENDS debug2c
  COMMENT "[DEBUG2C] Scanning for debugging variables"
  )
DISTCLEAN(${DEBUG_CMD_CPP})
set_source_files_properties(debug.cpp PROPERTIES OBJECT_DEPENDS ${DEBUG_CMD_CPP})

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-debug SHARED debug.cpp)
target_link_libraries(ged-debug libged libbu)
set_property(TARGET ged-debug APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-debug debug.cpp)
PLUGIN_SETUP(ged-debug ged)

CMAKEFILES(
  CMakeLists.txt
  debug.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
