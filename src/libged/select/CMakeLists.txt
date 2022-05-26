include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(SELECT_SRCS
  select.c
  select2.cpp
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-select SHARED ${SELECT_SRCS})
target_link_libraries(ged-select libged libbu)
set_property(TARGET ged-select APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-select ${SELECT_SRCS})
PLUGIN_SETUP(ged-select ged)

CMAKEFILES(
  CMakeLists.txt
  ${SELECT_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
