include_directories(
	${ZLIB_INCLUDE_DIR}
	${OPENNURBS_INCLUDE_DIR}
	${REGEX_INCLUDE_DIR}
	${TCL_INCLUDE_DIRS}
)

IF(MSVC)
	SET(WIN32LIBS ws2_32.lib Rpcrt4.lib)
	add_definitions(
		-DON_DLL_IMPORTS
		)
ENDIF(MSVC)


#add_executable(iges
#    BrepHandler.cpp
#    brlcad_brep.cpp
#    n_iges.cpp
#    n_main.cpp
#)
#target_link_libraries(iges libwdb librt libbu ${OPENNURBS_LIBRARY} ${WIN32LIBS})
#INSTALL(TARGETS iges RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_executable(iges-g
    add_face.c
    add_inner_shell.c
    add_loop.c
    b-spline.c
    block2.c
    brep.c
    brlabs.c
    check_names.c
    cone.c
    conv_drawings.c
    convassem.c
    convinst.c
    convsolids.c
    convsurf.c
    convtree.c
    cyl.c
    do_subfigs.c
    docolor.c
    ell.c
    evalxform.c
    extrudcirc.c
    extrudcon.c
    extrude.c
    findp.c
    freetree.c
    get_att.c
    get_cnurb_curve.c
    get_edge_list.c
    get_iges_vertex.c
    get_outer_shell.c
    get_vertex.c
    get_vertex_list.c
    getcurve.c
    iges_types.c
    main.c
    make_face.c
    make_nurb_face.c
    makedir.c
    makegroup.c
    makemembers.c
    matmult.c
    orient_loops.c
    planar_nurb.c
    read_att.c
    read_edge_list.c
    read_vertex_list.c
    readcnv.c
    readcols.c
    readdbl.c
    readflt.c
    readglobal.c
    readint.c
    readmatrix.c
    readname.c
    readrec.c
    readstart.c
    readstrg.c
    readtime.c
    readtree.c
    recsize.c
    revolve.c
    showtree.c
    sphere.c
    spline.c
    splinef.c
    stack.c
    summary.c
    tor.c
    trimsurf.c
    usage.c
    wedge.c
)
target_link_libraries(iges-g libwdb librt libbn libbu)
INSTALL(TARGETS iges-g RUNTIME DESTINATION bin)

add_executable(g-iges
    g-iges.c
    iges.c
)
target_link_libraries(g-iges librt libbu)
INSTALL(TARGETS g-iges RUNTIME DESTINATION bin)

SET(iges_MANS
  g-iges.1
  iges-g.1
)
install(FILES ${iges_MANS} DESTINATION ${MAN_DIR}/man1)
