add_subdirectory(core)

set(IGES_PLUGIN_SRCS iges_plugin.cpp iges_read.cpp iges_write.cpp)
gcv_plugin_library(gcv-iges SHARED ${IGES_PLUGIN_SRCS})
target_link_libraries(gcv-iges libgcv gcv_iges_core)
set_target_properties(gcv-iges PROPERTIES UNITY_BUILD OFF)
set_property(TARGET gcv-iges APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-iges "${IGES_PLUGIN_SRCS}")
plugin_setup(gcv-iges gcv)

add_subdirectory(tests)

cmakefiles(
  CMakeLists.txt
  iges_plugin.h
  ${IGES_PLUGIN_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
