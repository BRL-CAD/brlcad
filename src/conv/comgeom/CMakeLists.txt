include_directories(
    ../../../include
    ../../other/libz
    ../../other/openNURBS
    ../../other/libregex
    ../../other/tcl/generic
)

add_definitions(
    -DBRLCAD_DLL
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
    -DON_DLL_IMPORTS
)

add_executable(comgeom-g
    cvt.c
    f2a.c
    mat.c
    read.c
    region.c
    solid.c
    tools.c
)

target_link_libraries(comgeom-g
    BrlcadCore
)
