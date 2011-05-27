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
    extr.c 
    id.c 
    mater.c 
    nmg.c 
    nurb.c 
    pipe.c 
    reg.c 
    skt.c 
    submodel.c 
    units.c 
    vol.c 
    wdb.c
)

include_directories(
    ${REGEX_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
    ${OPENNURBS_INCLUDE_DIR}
    ${TCL_INCLUDE_DIRS}
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
