# Include directories needed by libbn users
set(BN_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BU_INCLUDE_DIRS}
	)
BRLCAD_INCLUDE_DIRS(BN_INCLUDE_DIRS)

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
if(CPP_DLL_DEFINES)
	SET_PROPERTY(TARGET bntester APPEND PROPERTY COMPILE_DEFINITIONS "BU_DLL_IMPORTS")
	SET_PROPERTY(TARGET bntester APPEND PROPERTY COMPILE_DEFINITIONS "BN_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)

CMAKEFILES(
        Makefile.am
        README
        bntester.dat
        ulp.c
)
