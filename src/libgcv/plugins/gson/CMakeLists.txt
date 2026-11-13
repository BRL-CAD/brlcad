gcv_plugin_library(gcv-gson SHARED gson.cpp)
target_link_libraries(gcv-gson libgcv libwdb librt libbu)
set_property(TARGET gcv-gson APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-gson gson.cpp)
plugin_setup(gcv-gson gcv)

cmakefiles(CMakeLists.txt gson.cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
