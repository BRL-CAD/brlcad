include_directories(
    ../../../include
    ../../other/libz
    ../../other/openNURBS
    ../../other/libregex
    ../../other/tcl/generic
)

add_definitions(
    -DBRLCAD_DLL
    -DUSE_PROTOTYPES
    -DON_DLL_IMPORTS
)

add_executable(3dm-g
    3dm-g.cpp
)

target_link_libraries(3dm-g
    BrlcadCore
    openNURBS
)
