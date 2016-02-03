include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    )


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


LIBGCV_ADD_PLUGIN(vrml "${LIBGCV_VRML_SOURCES}" "")


CMAKEFILES(
    vrml_read/file_util.h
    vrml_read/node.h
    vrml_read/node_type.h
    vrml_read/parser.h
    vrml_read/string_util.h
    vrml_read/transform_node.h
    )
