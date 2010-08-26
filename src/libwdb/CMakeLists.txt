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
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BRLCAD_REGEX_INCLUDE_DIR}
    ${BRLCAD_ZLIB_INCLUDE_DIR}
    ${BRLCAD_OPENNURBS_INCLUDE_DIR}
    ${TCL_INCLUDE_PATH}
)

if(MSVC)
   add_definitions(
       -DHAVE_CONFIG_H
       -DBRLCADBUILD
       -DOBJ_BREP=1
       -DON_DLL_IMPORTS
   )
else(MSVC)
   add_definitions(
       -DHAVE_CONFIG_H
       -DOBJ_BREP=1
       -DBRLCADBUILD
   )
endif(MSVC)

add_library(wdb ${LIBWDB_SOURCES})
target_link_libraries(wdb librt bu bn sysv ${BRLCAD_OPENNURBS_LIBRARY} ${BRLCAD_REGEX_LIBRARY} ${BRLCAD_ZLIB_LIBRARY} ${TCL_LIBRARY})
INSTALL(TARGETS wdb LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})
