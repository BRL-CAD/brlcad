set(EXTGL_SRCS dm-extgl-plugin.cpp)

if(BRLCAD_ENABLE_OPENGL AND OPENGL_TARGETS)
  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BRLCAD_SOURCE_DIR}/src/libbu
    ${BU_INCLUDE_DIRS}
  )

  dm_plugin_library(dm-extgl SHARED ${EXTGL_SRCS})
  target_link_libraries(dm-extgl libdmgl libdm libbu ${OPENGL_TARGETS})
  set_property(TARGET dm-extgl APPEND PROPERTY COMPILE_DEFINITIONS
    DM_PLUGIN BRLCADBUILD HAVE_CONFIG_H
  )
  validate_style(dm-extgl "${EXTGL_SRCS}")
  plugin_setup(dm-extgl dm)
endif()

cmakefiles(CMakeLists.txt ${EXTGL_SRCS})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
