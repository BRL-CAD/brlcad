include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${BRLCAD_SOURCE_DIR}/src/libbu # for fort.h
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-stat SHARED stat.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../libbu/fort.c)
target_link_libraries(ged-stat libged libbu)
set_property(TARGET ged-stat APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-stat stat.cpp)
PLUGIN_SETUP(ged-stat ged)

CMAKEFILES(
  CMakeLists.txt
  stat.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
