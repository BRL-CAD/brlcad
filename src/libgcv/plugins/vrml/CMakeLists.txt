set(VRML_SRCS
  vrml_read/file_util.cpp
  vrml_read/node.cpp
  vrml_read/node_type.cpp
  vrml_read/parser.cpp
  vrml_read/string_util.cpp
  vrml_read/transform_node.cpp
  vrml_read/vrml_read.cpp
  vrml_write.c
  )

gcv_plugin_library(gcv-vrml SHARED ${VRML_SRCS})
target_link_libraries(gcv-vrml librt libbn libbu)
set_property(TARGET gcv-vrml APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(gcv-vrml "${VRML_SRCS}")
PLUGIN_SETUP(gcv-vrml gcv)

CMAKEFILES(
  CMakeLists.txt
  ${VRML_SRCS}
  vrml_read/file_util.h
  vrml_read/node.h
  vrml_read/node_type.h
  vrml_read/parser.h
  vrml_read/string_util.h
  vrml_read/transform_node.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

