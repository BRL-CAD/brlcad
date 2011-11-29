
SUBDIRS = conf

brlcadincludedir = $(includedir)/brlcad

brlcadinclude_required = \
	RtServerImpl.h \
	bn.h \
	brlcad_config.h \
	bu.h \
	common.h \
	config_win.h \
	ged.h \
	magic.h \
	pstdint.h \
	raytrace.h \
	rt/defines.h \
	rtfunc.h \
	rtgeom.h \
	rtserver.h \
	tclcad.h \
	tie.h

brlcadinclude_wanted = \
	analyze.h \
	anim.h \
	bot.h \
	brep.h \
	brlcad.h \
	cmd.h \
	db.h \
	db5.h \
	dg.h \
	dm-Null.h \
	dm-X.h \
	dm-glx.h \
	dm-ogl.h \
	dm-pex.h \
	dm-plot.h \
	dm-ps.h \
	dm-rtgl.h \
	dm-tk.h \
	dm-wgl.h \
	dm.h \
	dm_color.h \
	dm_xvars.h \
	dvec.h \
	fb.h \
	fbio.h \
	fbserv_obj.h \
	fft.h \
	gcv.h \
	libtermio.h \
	light.h \
	mater.h \
	nmg.h \
	nurb.h \
	optical.h \
	orle.h \
	photonmap.h \
	pc.h \
	pkg.h \
	plastic.h \
	plot3.h \
	redblack.h \
	shadefuncs.h \
	shadework.h \
	solid.h \
	spectrum.h \
	spm.h \
	tol.h \
	vectfont.h \
	vector_fpu.h \
	vector_x86.h \
	vfont-if.h \
	vmath.h \
	wdb.h

# headers used by multiple packages but still considered private.
# public headers should NOT include these headers.
noinst_HEADERS = \
	bin.h \
	bio.h \
	brlcad_version.h \
	bselect.h \
	conf.h \
	fbmsg.h \
	sysv.h

EXTRA_DIST = \
	config_win_cmake.h.in \
	CMakeLists.txt

if ONLY_BENCHMARK
brlcadinclude_HEADERS = $(brlcadinclude_required)

else
# else !ONLY_BENCHMARK

if ONLY_RTS
brlcadinclude_HEADERS = $(brlcadinclude_required)

else
# else !ONLY_RTS
brlcadinclude_HEADERS = \
	$(brlcadinclude_required) \
	$(brlcadinclude_wanted)

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK


include $(top_srcdir)/misc/Makefile.defs
