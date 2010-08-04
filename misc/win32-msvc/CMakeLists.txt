cmake_minimum_required(VERSION 2.6)

project(BRL-CAD)

set(BUILD_SHARED_LIBS OFF)

add_definitions(
    -DOBJ_BREP
    -DSTDC_HEADERS
)

add_subdirectory(../../src/other/libregex ./libregex)
add_subdirectory(../../src/other/libz ./libz)
add_subdirectory(../../src/other/openNURBS ./openNURBS)
add_subdirectory(../../src/other/tcl ./tcl)

add_subdirectory(../../include/conf ./conf)
add_subdirectory(../../src/libbn ./libbn)
add_subdirectory(../../src/libbu ./libbu)
add_subdirectory(../../src/libicv ./libicv)
add_subdirectory(../../src/librt ./librt)
add_subdirectory(../../src/libgcv ./libgcv)
add_subdirectory(../../src/libged ./libged)
add_subdirectory(../../src/libsysv ./libsysv)
add_subdirectory(../../src/libwdb ./libwdb)

# build DLL
add_subdirectory(Dll ./Dll)

# build tools
add_subdirectory(../../src/conv ./conv)
add_subdirectory(../../src/conv/iges ./iges)
add_subdirectory(../../src/conv/intaval ./intaval)
add_subdirectory(../../src/nirt ./nirt)
