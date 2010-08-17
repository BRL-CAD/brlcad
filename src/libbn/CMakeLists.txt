set(LIBBN_SOURCES
    anim.c
    axis.c
    complex.c
    font.c
    fortran.c
    globals.c
    list.c
    marker.c
    mat.c
    msr.c
    multipoly.c
    noise.c
    plane.c
    plot3.c
    poly.c
    qmath.c
    rand.c
    randmt.c
    scale.c
    sphmap.c
    symbol.c
    tabdata.c
    tcl.c
    tplot.c
    vectfont.c
    vector.c
    vers.c
    vert_tree.c
    wavelet.c
)

include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(bn ${LIBBN_SOURCES})
target_link_libraries(bn bu)
INSTALL(TARGETS bn LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})
