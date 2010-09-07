set(RT_COMMON_SRCS
        do.c 
        heatgraph.c 
        main.c 
        opt.c 
        worker.c
)

set(rtexample_SRCS rtexample.c)

set(reshoot_SRCS reshoot.c)

set(rtwalk_SRCS rtwalk.c)

set(rtshot_SRCS rtshot.c)

set(hurt_SRCS hurt.c)

set(rtmlt_SRCS
    ${RT_COMMON_SRCS}
    scanline.c
    viewmlt.c
)

set(rtregis_SRCS
    read-rtlog.c
    rtregis.c
)

set(rtscale_SRCS
    read-rtlog.c
    rtscale.c
)

set(rt_SRCS
    ${RT_COMMON_SRCS}
    scanline.c
    view.c
)

set(rt_bot_faces_SRCS 
        ${RT_COMMON_SRCS}
        view_bot_faces.c
)

set(rtarea_SRCS 
        ${RT_COMMON_SRCS}
        viewarea.c
)

set(rtcell_SRCS 
        ${RT_COMMON_SRCS}
        viewcell.c
)

set(rtcheck_SRCS 
        ${RT_COMMON_SRCS}
        viewcheck.c
)

set(rtedge_SRCS 
        ${RT_COMMON_SRCS}
        viewedge.c
)

set(rtfrac_SRCS 
        ${RT_COMMON_SRCS}
        viewfrac.c
)

set(rtg3_SRCS 
        ${RT_COMMON_SRCS}
        viewg3.c
)

set(rthide_SRCS 
        ${RT_COMMON_SRCS}
        viewhide.c
)

set(rtpp_SRCS 
        ${RT_COMMON_SRCS}
        viewpp.c
)

set(rtrad_SRCS 
        ${RT_COMMON_SRCS}
        viewrad.c
)

set(rtrange_SRCS 
        ${RT_COMMON_SRCS}
        viewrange.c
)

set(rtray_SRCS 
        ${RT_COMMON_SRCS}
        viewray.c
)

set(rtsil_SRCS 
        ${RT_COMMON_SRCS}
        viewsil.c
)

set(rtweight_SRCS 
        ${RT_COMMON_SRCS}
        viewweight.c
)

set(rtxray_SRCS 
        ${RT_COMMON_SRCS}
        viewxray.c
)

set(LIBREMRT_SOURCES
        do.c 
        heatgraph.c 
        opt.c 
        scanline.c 
        sh_tcl.c 
        view.c 
        worker.c
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BRLCAD_PNG_INCLUDE_DIR}
    ${BRLCAD_ZLIB_INCLUDE_DIR}
    ${TCL_INCLUDE_PATH}
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD=1
)

add_executable(rtexample "${rtexample_SRCS}")
target_link_libraries(rtexample fb librt optical)

BRLCAD_ADDEXEC(reshoot "${reshoot_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtwalk "${rtwalk_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtshot "${rtshot_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(hurt "${hurt_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtmlt "${rtmlt_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtregis "${rtregis_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtscale "${rtscale_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rt "${rt_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rt_bot_faces "${rt_bot_faces_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtarea "${rtarea_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtcell "${rtcell_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtcheck "${rtcheck_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtedge "${rtedge_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtfrac "${rtfrac_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtg3 "${rtg3_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rthide "${rthide_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtpp "${rtpp_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtrad "${rtrad_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtrange "${rtrange_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtray "${rtray_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtsil "${rtsil_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtweight "${rtweight_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

BRLCAD_ADDEXEC(rtxray "${rtxray_SRCS}" "${CMAKE_THREAD_LIBS_INIT} fb librt optical")

add_library(libremrt STATIC ${LIBREMRT_SOURCES})
INSTALL(TARGETS libremrt ARCHIVE DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

add_library(librttherm STATIC ${RT_COMMON_SRCS})
INSTALL(TARGETS librttherm ARCHIVE DESTINATION ${BRLCAD_INSTALL_LIB_DIR})
SET_TARGET_PROPERTIES(librttherm PROPERTIES LINKER_LANGUAGE C)

SET(rt_MANS
  reshoot.1 
  rt_bot_faces.1 
  rtg3.1 
  rthide.1 
  rtpp.1 
  rtrange.1 
  rtray.1 
  rtregis.1 
  rtscale.1 
  rtshot.1 
  rtwalk.1 
  rtweight.1 
  rtxray.1
)
install(FILES ${rt_MANS} DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
