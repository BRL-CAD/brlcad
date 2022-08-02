include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-v2m_point SHARED v2m_point.c)
target_link_libraries(ged-v2m_point libged libbu)
set_property(TARGET ged-v2m_point APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-v2m_point v2m_point.c)
PLUGIN_SETUP(ged-v2m_point ged)

CMAKEFILES(
  CMakeLists.txt
  v2m_point.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
