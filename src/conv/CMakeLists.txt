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

add_executable(asc2g
    asc2g.c
)
target_link_libraries(asc2g
    BrlcadCore
    tcl85
)

add_executable(g2asc
    g2asc.c
)
target_link_libraries(g2asc
    BrlcadCore
    tcl85
)

add_executable(fast4-g
    fast4-g.c
)
target_link_libraries(fast4-g
    BrlcadCore
)

add_executable(stl-g
    stl-g.c
)
target_link_libraries(stl-g
    BrlcadCore
)

add_executable(g-stl
    g-stl.c
)
target_link_libraries(g-stl
    BrlcadCore
)

add_executable(dbupgrade
    dbupgrade.c
)
target_link_libraries(dbupgrade
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

add_executable(g4-g5
    g4-g5.c
)
target_link_libraries(g4-g5
    BrlcadCore
)

add_executable(g-acad
    g-acad.c
)
target_link_libraries(g-acad
    BrlcadCore
)

add_executable(g-obj
    g-obj.c
)
target_link_libraries(g-obj
    BrlcadCore
)

add_executable(g-tankill
    g-tankill.c
)
target_link_libraries(g-tankill
    BrlcadCore
)

add_executable(tankill-g
    tankill-g.c
)
target_link_libraries(tankill-g
    BrlcadCore
)

add_executable(g-var
    g-var.c
)
target_link_libraries(g-var
    BrlcadCore
)

add_executable(g-vrml
    g-vrml.c
)
target_link_libraries(g-vrml
    BrlcadCore
)

add_executable(g-x3d
    g-x3d.c
)
target_link_libraries(g-x3d
    BrlcadCore
)

add_executable(nastran-g
    nastran-g.c
)
target_link_libraries(nastran-g
    BrlcadCore
)

add_executable(ply-g
    ply-g.c
)
target_link_libraries(ply-g
    BrlcadCore
)

add_executable(proe-g
    proe-g.c
)
target_link_libraries(proe-g
    BrlcadCore
)

add_executable(viewpoint-g
    viewpoint-g.c
)
target_link_libraries(viewpoint-g
    BrlcadCore
)

add_executable(g-xxx_facets
    g-xxx_facets.c
)
target_link_libraries(g-xxx_facets
    BrlcadCore
)
