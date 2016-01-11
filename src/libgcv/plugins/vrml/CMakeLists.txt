include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    )


set(GCV_VRML_SOURCES
    vrml_read/file_util.cpp
    vrml_read/node.cpp
    vrml_read/node_type.cpp
    vrml_read/parser.cpp
    vrml_read/string_util.cpp
    vrml_read/transform_node.cpp
    vrml_read/vrml_read.cpp
    vrml_write.c
    )


BRLCAD_ADDLIB(gcv_vrml "${GCV_VRML_SOURCES}" "librt;libwdb;libbu;" STATIC NO_INSTALL)
set_property(TARGET gcv_vrml-static PROPERTY POSITION_INDEPENDENT_CODE ON)


CMAKEFILES(
    vrml_read/file_util.h
    vrml_read/node.h
    vrml_read/node_type.h
    vrml_read/parser.h
    vrml_read/string_util.h
    vrml_read/transform_node.h
    )
