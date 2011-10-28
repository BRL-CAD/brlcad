# Include directories needed by libwdb users
SET(WDB_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BU_INCLUDE_DIRS}
	${BN_INCLUDE_DIRS}
	${RT_INCLUDE_DIRS}
	${SYSV_INCLUDE_DIRS}
	)
BRLCAD_INCLUDE_DIRS(WDB_INCLUDE_DIRS)

set(LIBWDB_SOURCES
    arbn.c
    ars.c
    bot.c
    brep.cpp
    cline.c
    constraint.c
    dsp.c
    dynamic_geometry.c
    ebm.c
    export.c
    extrude.c
    id.c
    mater.c
    nmg.c
    nurb.c
    pipe.c
    reg.c
    sketch.c
    submodel.c
    units.c
    vol.c
    wdb.c
)

if(MSVC)
   add_definitions(
       -DOBJ_BREP=1
       -DON_DLL_IMPORTS
   )
else(MSVC)
   add_definitions(
       -DOBJ_BREP=1
   )
endif(MSVC)

BRLCAD_ADDLIB(libwdb "${LIBWDB_SOURCES}" "librt libbn libbu libsysv ${OPENNURBS_LIBRARY} ${REGEX_LIBRARY} ${ZLIB_LIBRARY}")
SET_TARGET_PROPERTIES(libwdb PROPERTIES VERSION 19.0.1 SOVERSION 19)

ADD_MAN_PAGE(3 libwdb.3)
CMAKEFILES(Makefile.am)
