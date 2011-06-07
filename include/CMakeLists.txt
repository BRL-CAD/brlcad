SET(brlcadinclude_required
	RtServerImpl.h 
	bn.h
	${CMAKE_CURRENT_BINARY_DIR}/brlcad_config.h
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
	fft.h
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
	tie.h
	vectfont.h 
	vector_fpu.h 
	vector_x86.h 
	vfont-if.h 
	vmath.h 
	wdb.h
)

# Just add the osl-renderer.h if we are going to enable OSL
IF(BRLCAD-ENABLE_OSL)
    SET(brlcadinclude_wanted ${brlcadinclude_wanted} osl-renderer.h)
ENDIF(BRLCAD-ENABLE_OSL)


# headers used by multiple packages but still considered private.
# public headers should NOT include these headers.
SET(brlcadnoinst_HEADERS
	brlcad_version.h 
	bin.h
	bio.h 
	conf.h 
	fbmsg.h 
	machine.h 
	sysv.h
)

INSTALL(FILES ${brlcadinclude_required} DESTINATION include/brlcad)

IF(NOT BRLCAD-BUILD_BENCHMARK_ONLY AND NOT BRLCAD-BUILD_RTSERVER_ONLY)
	INSTALL(FILES ${brlcadinclude_wanted} DESTINATION include/brlcad)
ENDIF(NOT BRLCAD-BUILD_BENCHMARK_ONLY AND NOT BRLCAD-BUILD_RTSERVER_ONLY)
