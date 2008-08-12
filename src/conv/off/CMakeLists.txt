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

add_executable(g-off
    g-off.c
)
target_link_libraries(g-off
    BrlcadCore
)

add_executable(off-g
    off-g.c
)
target_link_libraries(off-g
    BrlcadCore
)
