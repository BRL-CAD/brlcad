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

add_executable(asc2g asc2g.c)
target_link_libraries(asc2g BrlcadCore tcl85)

add_executable(fast4-g fast4-g.c)
target_link_libraries(fast4-g BrlcadCore)

add_executable(g2asc g2asc.c)
target_link_libraries(g2asc BrlcadCore tcl85)

add_executable(proe-g proe-g.c)
target_link_libraries(proe-g BrlcadCore)
