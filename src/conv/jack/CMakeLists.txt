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

add_executable(jack-g
    jack-g.c
)
target_link_libraries(jack-g
    BrlcadCore
)

add_executable(g-jack
    g-jack.c
)
target_link_libraries(g-jack
    BrlcadCore
)
