SET(brlcadinclude_required
	RtServerImpl.h 
	bn.h 
	bu.h 
	common.h 
	config_win_cmake.h 
	ged.h 
	magic.h 
	pstdint.h 
	raytrace.h 
	rtfunc.h 
	rtgeom.h 
	rtserver.h 
	tclcad.h
)

SET(brlcadinclude_wanted
	analyze.h 
	anim.h 
	bot.h 
	brep.h 
	brlcad.h 
	cmd.h 
	db.h 
	db5.h 
	dg.h 
	dm-Null.h 
	dm-X.h 
	dm-glx.h 
	dm-ogl.h 
	dm-pex.h 
	dm-plot.h 
	dm-ps.h 
	dm-rtgl.h 
	dm-tk.h 
	dm-wgl.h 
	dm.h 
	dm_color.h 
	dm_xvars.h 
	dvec.h 
	fb.h 
	fbio.h 
	fbserv_obj.h 
	gcv.h 
	libtermio.h 
	light.h 
	mater.h 
	nmg.h 
	nurb.h 
	optical.h 
	orle.h 
	photonmap.h 
	pc.h 
	pkg.h 
	plastic.h 
	plot3.h 
	redblack.h 
	shadefuncs.h 
	shadework.h 
	solid.h 
	spectrum.h 
	spm.h 
	vectfont.h 
	vector_fpu.h 
	vector_x86.h 
	vfont-if.h 
	vmath.h 
	wdb.h
)

# headers used by multiple packages but still considered private.
# public headers should NOT include these headers.
SET(brlcadnoinst_HEADERS
	brlcad_version.h 
	bio.h 
	conf.h 
	fbmsg.h 
	machine.h 
	sysv.h
)

FILE(COPY ${brlcadinclude_required} DESTINATION	${CMAKE_BINARY_DIR}/include/brlcad)
INSTALL(FILES ${brlcadinclude_required} DESTINATION include/brlcad)
INSTALL(FILES ${CMAKE_BINARY_DIR}/include/brlcad/brlcad_config.h DESTINATION include/brlcad)

IF(NOT BRLCAD-BUILD_BENCHMARK_ONLY AND NOT BRLCAD-BUILD_RTSERVER_ONLY)
	FILE(COPY ${brlcadinclude_wanted} DESTINATION ${CMAKE_BINARY_DIR}/include/brlcad)
	INSTALL(FILES ${brlcadinclude_wanted} DESTINATION include/brlcad)
ENDIF(NOT BRLCAD-BUILD_BENCHMARK_ONLY AND NOT BRLCAD-BUILD_RTSERVER_ONLY)
