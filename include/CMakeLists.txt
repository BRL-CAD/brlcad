SET(brlcadinclude_required
	${CMAKE_CURRENT_BINARY_DIR}/brlcad_config.h
	RtServerImpl.h
	bn.h
	bu.h
	common.h
	ged.h
	magic.h
	pstdint.h
	raytrace.h
	rt/defines.h
	rtfunc.h
	rtgeom.h
	rtserver.h
	tclcad.h
)
IF(WIN32)
	SET(brlcadinclude_required ${brlcadinclude_required} ${CMAKE_CURRENT_BINARY_DIR}/config_win_cmake.h)
ENDIF(WIN32)

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
        obj.h
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
        tol.h
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
	bin.h
	bio.h
	brlcad_version.h
	bselect.h
	conf.h
	fbmsg.h
	sysv.h
)

SET(brlcadinclude_misc
	conf
	config_win.h
	config_win_cmake.h.in
	)

INSTALL(FILES ${brlcadinclude_required} DESTINATION include/brlcad)


IF(NOT BRLCAD_BUILD_BENCHMARK_ONLY AND NOT BRLCAD_BUILD_RTSERVER_ONLY)
	INSTALL(FILES ${brlcadinclude_wanted} DESTINATION include/brlcad)
ENDIF(NOT BRLCAD_BUILD_BENCHMARK_ONLY AND NOT BRLCAD_BUILD_RTSERVER_ONLY)

CMAKEFILES(${brlcadinclude_required})
CMAKEFILES(${brlcadinclude_wanted})
CMAKEFILES(${brlcadnoinst_HEADERS})
CMAKEFILES(${brlcadinclude_misc})
CMAKEFILES(Makefile.am)
