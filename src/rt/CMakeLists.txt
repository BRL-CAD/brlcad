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

add_executable(rtexample ${rtexample_SRCS})
target_link_libraries(rtexample ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
INSTALL(TARGETS rtexample RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(reshoot ${reshoot_SRCS})
target_link_libraries(reshoot ${CMAKE_THREAD_LIBS_INIT} fb librt optical)

add_executable(rtwalk ${rtwalk_SRCS})
target_link_libraries(rtwalk ${CMAKE_THREAD_LIBS_INIT} fb librt optical)

add_executable(rtshot ${rtshot_SRCS})
target_link_libraries(rtshot ${CMAKE_THREAD_LIBS_INIT} fb librt optical)

add_executable(hurt ${hurt_SRCS})
target_link_libraries(hurt ${CMAKE_THREAD_LIBS_INIT} fb librt optical)

add_executable(rtmlt ${rtmlt_SRCS})
target_link_libraries(rtmlt ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtregis ${rtregis_SRCS})
target_link_libraries(rtregis ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtscale ${rtscale_SRCS})
target_link_libraries(rtscale ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rt ${rt_SRCS})
target_link_libraries(rt ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rt_bot_faces ${rt_bot_faces_SRCS})
target_link_libraries(rt_bot_faces ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtarea ${rtarea_SRCS})
target_link_libraries(rtarea ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtcell ${rtcell_SRCS})
target_link_libraries(rtcell ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtcheck ${rtcheck_SRCS})
target_link_libraries(rtcheck ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtedge ${rtedge_SRCS})
target_link_libraries(rtedge ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtfrac ${rtfrac_SRCS})
target_link_libraries(rtfrac ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtg3 ${rtg3_SRCS})
target_link_libraries(rtg3 ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rthide ${rthide_SRCS})
target_link_libraries(rthide ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtpp ${rtpp_SRCS})
target_link_libraries(rtpp ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtrad ${rtrad_SRCS})
target_link_libraries(rtrad ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtrange ${rtrange_SRCS})
target_link_libraries(rtrange ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtray ${rtray_SRCS})
target_link_libraries(rtray ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtsil ${rtsil_SRCS})
target_link_libraries(rtsil ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtweight ${rtweight_SRCS})
target_link_libraries(rtweight ${CMAKE_THREAD_LIBS_INIT} fb librt optical)
add_executable(rtxray ${rtxray_SRCS})
target_link_libraries(rtxray ${CMAKE_THREAD_LIBS_INIT} fb librt optical)

add_library(remrt STATIC ${LIBREMRT_SOURCES})
