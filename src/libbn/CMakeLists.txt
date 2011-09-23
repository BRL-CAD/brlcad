include_directories(
	${BU_INCLUDE_DIRS}
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


BRLCAD_ADDLIB(libbn "${LIBBN_SOURCES}" libbu)
SET_TARGET_PROPERTIES(libbn PROPERTIES VERSION 19.0.1 SOVERSION 19)

ADD_MAN_PAGE(3 libplot3.3)

add_executable(bntester bntester.c)
target_link_libraries(bntester libbu libbn)
IF(MSVC)
	SET_TARGET_PROPERTIES(bntester PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL")
ENDIF(MSVC)
CMAKEFILES(bntester.dat ulp.c)
CMAKEFILES(Makefile.am)
