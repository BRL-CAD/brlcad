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

add_executable(bot-bldxf
    bot-bldxf.c
)
target_link_libraries(bot-bldxf
    BrlcadCore
)

add_executable(dxf-g
    dxf-g.c
)
target_link_libraries(dxf-g
    BrlcadCore
)

add_executable(g-dxf
    g-dxf.c
)
target_link_libraries(g-dxf
    BrlcadCore
)
