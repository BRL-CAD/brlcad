# $Id$

brlcadincludedir = $(includedir)/brlcad

brlcadinclude_required = \
	bn.h \
	bu.h \
	config.h \
	raytrace.h \
	rtgeom.h \
	rtlist.h \
	rtprivate.h \
	rtstring.h

brlcadinclude_wanted = \
	XtndRunsv.h \
	anim.h \
	brlregex.h \
	cmd.h \
	compat4.h \
	db.h \
	db5.h \
	dm-Null.h \
	dm-X.h \
	dm-glx.h \
	dm-ogl.h \
	dm-pex.h \
	dm-plot.h \
	dm-ps.h \
	dm.h \
	dm_color.h \
	dm_xvars.h \
	externs.h \
	fb.h \
	fbserv_obj.h \
	forms.h \
	itcl.h \
	itclDecls.h \
	itclInt.h \
	itclIntDecls.h \
	itk.h \
	itkDecls.h \
	libtermio.h \
	light.h \
	machine.h \
	mater.h \
	msr.h \
	nmg.h \
	noalias-prag.h \
	noalias.h \
	nurb.h \
	orle.h \
	photonmap.h \
	pkg.h \
	plastic.h \
	plot3.h \
	png.h \
	pngconf.h \
	pyramid.h \
	redblack.h \
	rle.h \
	rle_code.h \
	rle_config.h \
	rle_getraw.h \
	rle_put.h \
	rle_raw.h \
	shadefuncs.h \
	shadework.h \
	shortvect-pr.h \
	shortvect.h \
	solid.h \
	spectrum.h \
	spm.h \
	svfb.h \
	svfb_global.h \
	tcl.h \
	tclDecls.h \
	tclInt.h \
	tclIntDecls.h \
	tclPlatDecls.h \
	tk.h \
	tkDecls.h \
	vectfont.h \
	vfont-if.h \
	vmath.h \
	wdb.h \
	zconf.h \
	zlib.h

if ONLYRTS
brlcadinclude_HEADERS = \
	$(brlcadinclude_required)
else
brlcadinclude_HEADERS = \
	$(brlcadinclude_required) \
	$(brlcadinclude_wanted)
endif
