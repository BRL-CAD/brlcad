include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

if(0)
add_definitions(-DGED_PLUGIN)
add_library(ged-simulate SHARED simulate.c)
target_link_libraries(ged-simulate libged libbu)
set_property(TARGET ged-simulate APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-simulate simulate.c)
PLUGIN_SETUP(ged-simulate ged)
endif(0)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
