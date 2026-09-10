set(IGES_CORE_SRCS
  iges_brep_export.cpp
  iges_brep_import.cpp
  iges_convert.cpp
  iges_document.cpp
  iges_export.cpp
  iges_import.cpp
  iges_native.cpp
  iges_nmg_export.cpp
  iges_orientation.cpp
  iges_primitive_export.cpp
  iges_report.cpp
  iges_runtime.cpp
  iges_wire.cpp
  iges_writer.cpp
)

# Private implementation shared by the loadable plugin and command-line tools.
# It is not installed and does not add an IGES interface to libgcv's public API.
add_library(gcv_iges_core STATIC ${IGES_CORE_SRCS})
target_include_directories(gcv_iges_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gcv_iges_core PUBLIC libwdb librt libnmg libbrep libbn libbu Threads::Threads ${M_LIBRARY})
target_compile_definitions(gcv_iges_core PRIVATE BRLCADBUILD HAVE_CONFIG_H)
set_target_properties(gcv_iges_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  UNITY_BUILD OFF
  FOLDER "BRL-CAD Plugins/gcv"
)
validate_style(gcv_iges_core "${IGES_CORE_SRCS}")

cmakefiles(${IGES_CORE_SRCS}
  CMakeLists.txt DESIGN.md iges_brep_import.h iges_convert.h iges_document.h
  iges_export.h iges_import.h iges_native.h iges_parameters.h iges_report.h
  iges_runtime.h iges_writer.h iges_orientation.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
