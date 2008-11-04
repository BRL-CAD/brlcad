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

add_executable(tgf-g
    read_dra.cpp
    regtab.cpp
    tgf-g.cpp
    write_brl.cpp
)
target_link_libraries(tgf-g
    BrlcadCore
)
