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

add_executable(3dm-g
    3dm/3dm-g.cpp
)
target_link_libraries(3dm-g
    BrlcadCore
    openNURBS
)

add_executable(asc2g
    asc/asc2g.c
)
target_link_libraries(asc2g
    BrlcadCore
    tcl85
    Ws2_32
)

add_executable(g2asc
    asc/g2asc.c
)
target_link_libraries(g2asc
    BrlcadCore
    tcl85
    Ws2_32
)

add_executable(comgeom-g
    comgeom/cvt.c
    comgeom/f2a.c
    comgeom/mat.c
    comgeom/read.c
    comgeom/region.c
    comgeom/solid.c
    comgeom/tools.c
)
target_link_libraries(comgeom-g
    BrlcadCore
)

add_executable(bot-bldxf
    dxf/bot-bldxf.c
)
target_link_libraries(bot-bldxf
    BrlcadCore
)

add_executable(dxf-g
    dxf/dxf-g.c
)
target_link_libraries(dxf-g
    BrlcadCore
)

add_executable(g-dxf
    dxf/g-dxf.c
)
target_link_libraries(g-dxf
    BrlcadCore
)

add_executable(fast4-g
    fast4-g.c
)
target_link_libraries(fast4-g
    BrlcadCore
)

add_executable(jack-g
    jack/jack-g.c
)
target_link_libraries(jack-g
    BrlcadCore
)

add_executable(g-jack
    jack/g-jack.c
)
target_link_libraries(g-jack
    BrlcadCore
)

add_executable(g-off
    off/g-off.c
)
target_link_libraries(g-off
    BrlcadCore
)

add_executable(off-g
    off/off-g.c
)
target_link_libraries(off-g
    BrlcadCore
)

add_executable(stl-g
    stl/stl-g.c
)
target_link_libraries(stl-g
    BrlcadCore
    Ws2_32
)

add_executable(g-stl
    stl/g-stl.c
)
target_link_libraries(g-stl
    BrlcadCore
    Ws2_32
)

add_executable(dbupgrade
    dbupgrade.c
)
target_link_libraries(dbupgrade
    BrlcadCore
)

add_executable(g4-g5
    g4-g5.c
)
target_link_libraries(g4-g5
    BrlcadCore
)

add_executable(g5-g4
    g5-g4.c
)
target_link_libraries(g5-g4
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

add_executable(obj-g
    obj-g.c
)
target_link_libraries(obj-g
    BrlcadCore
)

add_executable(patch-g
    patch/patch-g.c
)
target_link_libraries(patch-g
    BrlcadCore
)

add_executable(rpatch
    patch/rpatch.c
)
target_link_libraries(rpatch
    BrlcadCore
)

add_executable(g-tankill
    tankill/g-tankill.c
)
target_link_libraries(g-tankill
    BrlcadCore
)

add_executable(tankill-g
    tankill/tankill-g.c
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
