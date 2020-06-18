set(LIBGCV_VRML_SOURCES
  vrml_read/file_util.cpp
  vrml_read/node.cpp
  vrml_read/node_type.cpp
  vrml_read/parser.cpp
  vrml_read/string_util.cpp
  vrml_read/transform_node.cpp
  vrml_read/vrml_read.cpp
  vrml_write.c
  )


LIBGCV_ADD_PLUGIN(vrml "${LIBGCV_VRML_SOURCES}" "librt;libbn;libbu")

CMAKEFILES(
  CMakeLists.txt
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

