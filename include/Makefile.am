# $Id$

brlcadincludedir = $(includedir)/brlcad

brlcadinclude_required = \
	RtServerImpl.h \
	bn.h \
	brlcad_config.h \
	brlcad_version.h \
	bu.h \
	common.h \
	config_win.h \
	raytrace.h \
	rtgeom.h \
	rtlist.h \
	rtprivate.h \
	rtserver.h \
	rtstring.h \
	tclcad.h

headers_libitcl = \
	itcl.h \
	itclDecls.h \
	itclInt.h \
	itclIntDecls.h \
	itk.h \
	itkDecls.h

headers_libtcl = \
	tcl.h \
	tclDecls.h \
	tclInt.h \
	tclIntDecls.h \
	tclIntPlatDecls.h \
	tclPlatDecls.h

headers_libtk = \
	tk.h \
	tkDecls.h \
	tkInt.h \
	tkIntDecls.h \
	tkIntPlatDecls.h \
	tkIntXlibDecls.h \
	tkPlatDecls.h

brlcadinclude_wanted = \
	$(headers_libitcl) \
	$(headers_libtcl) \
	$(headers_libtk) \
	XtndRunsv.h \
	anim.h \
	brlcad.h \
	cmd.h \
	conf.h \
	compat4.h \
	db.h \
	db5.h \
	dm-Null.h \
	dm-X.h \
	dm-glx.h \
	dm-ogl.h \
	dm-wgl.h \
	dm-pex.h \
	dm-plot.h \
	dm-ps.h \
	dm-tk.h \
	dm.h \
	dm_color.h \
	dm_xvars.h \
	fb.h \
	fbio.h \
	fbserv_obj.h \
	libtermio.h \
	light.h \
	machine.h \
	mater.h \
	msr.h \
	nmg.h \
	noalias-prag.h \
	noalias.h \
	nurb.h \
	optical.h \
	orle.h \
	photonmap.h \
	pkg.h \
	plastic.h \
	plot3.h \
	redblack.h \
	regex.h \
	shadefuncs.h \
	shadework.h \
	shortvect-pr.h \
	shortvect.h \
	solid.h \
	spectrum.h \
	spm.h \
	svfb.h \
	svfb_global.h \
	vectfont.h \
	vfont-if.h \
	vmath.h \
	wdb.h

# headers used by multiple packages but still considered private
noinst_HEADERS = \
	fbmsg.h

EXTRA_DIST = \
	on_nurb.h \
	sed4

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

brlcad_version.h:
	@sh $(top_srcdir)/sh/vers.sh brlcad_version "BRL-CAD" > $@

# make sure that the build date/version is updated every time make is run
all-local:
	@sh $(top_srcdir)/sh/vers.sh brlcad_version "BRL-CAD" > brlcad_version.h

DISTCLEANFILES = \
	brlcad_version.h \
	stamp-h1 \
	version

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
