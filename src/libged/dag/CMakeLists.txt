include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

if(ADAPTAGRAMS_FOUND)
  set(LIBGED_CMD_LIBS_LIST ${LIBGED_CMD_LIBS_LIST} ${ADAPTAGRAMS_LIBRARIES})
  set(LIBGED_CMD_INCLUDE_DIRS ${LIBGED_CMD_INCLUDE_DIRS} ${AVOID_INCLUDE_DIR})
endif(ADAPTAGRAMS_FOUND)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-dag SHARED dag.cpp)
target_link_libraries(ged-dag libged libbu)
set_property(TARGET ged-dag APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-dag dag.cpp)
PLUGIN_SETUP(ged-dag ged)

CMAKEFILES(
  CMakeLists.txt
  dag.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
