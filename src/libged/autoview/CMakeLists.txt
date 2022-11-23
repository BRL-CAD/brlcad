include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-autoview SHARED autoview.c autoview2.cpp)
target_link_libraries(ged-autoview libged libbu)
set_property(TARGET ged-autoview APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-autoview autoview.c autoview2.cpp)
PLUGIN_SETUP(ged-autoview ged)

CMAKEFILES(
  CMakeLists.txt
  autoview.c
  autoview2.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
