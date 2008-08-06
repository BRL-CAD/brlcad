cmake_minimum_required(VERSION 2.6)

project(brlcad)

set(BUILD_SHARED_LIBS OFF)

add_definitions(-DOBJ_BREP)

add_subdirectory(../../src/other/libregex .)
add_subdirectory(../../src/other/libz .)
add_subdirectory(../../src/other/openNURBS .)
add_subdirectory(../../src/other/tcl .)

add_subdirectory(../../include/conf .)
add_subdirectory(../../src/libbn .)
add_subdirectory(../../src/libbu .)
add_subdirectory(../../src/librt .)
add_subdirectory(../../src/libged .)
add_subdirectory(../../src/libsysv .)
add_subdirectory(../../src/libwdb .)

# build DLL
add_subdirectory(Dll .)

# buld tools
add_subdirectory(../../src/conv .)
add_subdirectory(../../src/conv/3dm .)
