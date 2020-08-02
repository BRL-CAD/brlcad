include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-facetize SHARED facetize.cpp facetize_log.c)
target_link_libraries(ged-facetize libged libbu)
set_property(TARGET ged-facetize APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-facetize facetize.cpp)
PLUGIN_SETUP(ged-facetize ged)

CMAKEFILES(
  CMakeLists.txt
  facetize.cpp
  facetize_log.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
