include_directories(
    ${TCL_INCLUDE_DIRS}
)

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

IF(MSVC)
    add_definitions(
	    -DBN_EXPORT_DLL
	 )
ENDIF(MSVC)

BRLCAD_ADDLIB(libbn "${LIBBN_SOURCES}" libbu STRICT)
SET_TARGET_PROPERTIES(libbn PROPERTIES VERSION 19.0.1 SOVERSION 19)

install(FILES libplot3.3 DESTINATION ${MAN_DIR}/man3)

add_executable(bntester bntester.c)
target_link_libraries(bntester libbu libbn)
