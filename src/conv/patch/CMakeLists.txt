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
)

add_executable(patch-g patch-g.c)
target_link_libraries(patch-g BrlcadCore)

add_executable(rpatch rpatch.c)
target_link_libraries(rpatch BrlcadCore)
