set(IGES_CONVERTER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../conv/iges")
set(
  IGES_READER_SRCS
  iges_read.cpp
  "${IGES_CONVERTER_DIR}/iges_brep_import.cpp"
  "${IGES_CONVERTER_DIR}/iges_document.cpp"
  "${IGES_CONVERTER_DIR}/iges_import.cpp"
)

gcv_plugin_library(gcv-iges SHARED ${IGES_READER_SRCS})
target_include_directories(gcv-iges PRIVATE "${IGES_CONVERTER_DIR}")
target_link_libraries(gcv-iges libgcv libwdb librt libnmg libbrep libbn libbu ${M_LIBRARY})
set_target_properties(gcv-iges PROPERTIES UNITY_BUILD OFF)
set_property(TARGET gcv-iges APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-iges "iges_read.cpp")
plugin_setup(gcv-iges gcv)

cmakefiles(
  CMakeLists.txt
  iges_read.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
