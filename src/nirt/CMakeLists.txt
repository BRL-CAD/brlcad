include_directories(
    ../../include
    ../other/libz
    ../other/openNURBS
    ../other/libregex
    ../other/tcl/generic
)

add_definitions(
    -DBRLCAD_DLL
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
    -DON_DLL_IMPORTS
)

add_executable(nirt
    nirt.c
    command.c
    conversion.c
    if.c
    interact.c
    bsphere.c
    read_mat.c
    str_manip.c
    parse_fmt.c
)
target_link_libraries(nirt
    BrlcadCore
)

add_executable(showshot
    showshot.c
)
target_link_libraries(showshot
    BrlcadCore
)
