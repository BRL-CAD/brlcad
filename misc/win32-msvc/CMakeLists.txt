project(brlcad)

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(../../include/conf .)
add_subdirectory(../../src/libbn .)
add_subdirectory(../../src/libbu .)
add_subdirectory(../../src/librt .)
add_subdirectory(../../src/libsysv .)
add_subdirectory(../../src/libwdb .)
add_subdirectory(../../src/other/libregex .)
add_subdirectory(../../src/other/libz .)
add_subdirectory(../../src/other/openNURBS .)
add_subdirectory(Dll .)
