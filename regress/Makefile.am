.PHONY : regression regress test mged red moss lights solids shaders spdi iges weight gqa fastgen bots repository comgeom gdot gnff vls_vprintf dsp asc2dsp

SUBDIRS = mged

regression: clean mged moss lights solids shaders spdi iges weight gqa fastgen bots repository comgeom gdot gnff vls_vprintf dsp
	@${ECHO} ---
	@${ECHO} Regression testing completed.

regress: regression

test: regression

mged: $(top_srcdir)/regress/mged.sh
	-${SH} $(top_srcdir)/regress/mged.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

red: $(top_srcdir)/regress/red.sh
	-${SH} $(top_srcdir)/regress/red.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

moss: $(top_srcdir)/regress/moss.sh
	-${SH} $(top_srcdir)/regress/moss.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

lights: $(top_srcdir)/regress/lights.sh
	-${SH} $(top_srcdir)/regress/lights.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

solids: $(top_srcdir)/regress/solids.sh
	-${SH} $(top_srcdir)/regress/solids.sh $(top_srcdir) $(top_builddir)
	@${ECHO} +++ $@ test complete.

shaders: $(top_srcdir)/regress/shaders.sh
	-${SH} $(top_srcdir)/regress/shaders.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

spdi: $(top_srcdir)/regress/spdi.sh
	-${SH} $(top_srcdir)/regress/spdi.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

iges: $(top_srcdir)/regress/iges.sh
	-${SH} $(top_srcdir)/regress/iges.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

weight: $(top_srcdir)/regress/weight.sh
	-${SH} $(top_srcdir)/regress/weight.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

gqa: $(top_srcdir)/regress/gqa.sh
	-${SH} $(top_srcdir)/regress/gqa.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

fastgen: $(top_srcdir)/regress/fastgen.sh
	-${SH} $(top_srcdir)/regress/fastgen.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

bots: $(top_srcdir)/regress/bots.sh
	-${SH} $(top_srcdir)/regress/bots.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

#flawfinder: $(top_srcdir)/regress/flawfinder.sh
#	-${SH} $(top_srcdir)/regress/flawfinder.sh $(top_srcdir)
#	@${ECHO} +++ $@ test complete.

repository: $(top_srcdir)/regress/repository.sh
	-${SH} $(top_srcdir)/regress/repository.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

comgeom: $(top_srcdir)/regress/comgeom.sh
	-${SH} $(top_srcdir)/regress/comgeom.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

gdot: $(top_srcdir)/regress/g-dot.sh
	-${SH} $(top_srcdir)/regress/g-dot.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

gnff: $(top_srcdir)/regress/g-nff.sh
	-${SH} $(top_srcdir)/regress/g-nff.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

vls_vprintf: $(top_srcdir)/regress/vls_vprintf.sh
	-${SH} $(top_srcdir)/regress/vls_vprintf.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

dsp: $(top_srcdir)/regress/dsp.sh
	-${SH} $(top_srcdir)/regress/dsp.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

asc2dsp: $(top_srcdir)/regress/asc2dsp.sh
	-${SH} $(top_srcdir)/regress/asc2dsp.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

# these are here because they go in the distribution but are not installed
EXTRA_DIST = \
	asc2dsp.sh \
	bots.sh \
	comgeom.sh \
	dsp.sh \
	fastgen.sh \
	fastgen_dos.fast4 \
	flawfinder.sh \
	g-dot.sh \
	g-nff.sh \
	gqa.sh \
	iges.sh \
	library.sh \
	lights.sh \
	lights_ref.asc \
	main.sh \
	master_fetch.sh \
	master_prep.sh \
	mged.sh \
	mged_test.sh \
	moss.sh \
	mosspix.asc \
	nightly.sh \
	red.sh \
	repository.sh \
	shaders.sh \
	shaderspix.asc \
	slave_build.sh \
	solids.sh \
	solidspix.asc \
	spdi.sh \
	spdipix.asc \
	testlib.c \
	vls_vprintf.sh \
	weight.sh \
	CMakeLists.txt

CLEANFILES = \
	$(MOSTLYCLEANFILES) \
	.density \
	adj_air.pl \
	asc2dsp-new.dsp \
	asc2dsp-old.bw \
	asc2dsp-old.dsp \
	asc2dsp-old.pix \
	asc2dsp.log \
	bots.g \
	bots.lh.pix \
	bots.log \
	bots.no.pix \
	bots.rh.pix \
	bots.sph.pix \
	bots.diff.log \
	bots.diff.pix \
	bots.rl.diff.pix \
	bots.rn.diff.pix \
	bots.rs.diff.pix \
	bots.sync.pix \
	comgeom-g.log \
	comgeom.m35.asc \
	comgeom.m35-baseline.cg \
	comgeom.m35.cg \
	comgeom.m35.g \
	comgeom.t-v4.g \
	comgeom.t-v5.g \
	density_table.txt \
	dsp-2-1.bw \
	dsp-2-1.dsp \
	dsp-2-1.g \
	dsp-2-1.log \
	dsp-2-1.pix \
	dsp-2-1.rt.pix \
	dsp-2-2.bw \
	dsp-2-2.dsp \
	dsp-2-2.g \
	dsp-2-2.log \
	dsp-2-2.pix \
	dsp-2-2.rt.pix \
	dsp-2-3.bw \
	dsp-2-3.dsp \
	dsp-2-3.g \
	dsp-2-3.log \
	dsp-2-3.pix \
	dsp-2-3.rt.pix \
	dsp-2-4.bw \
	dsp-2-4.dsp \
	dsp-2-4.g \
	dsp-2-4.log \
	dsp-2-4.pix \
	dsp-2-4.rt.pix \
	dsp-2-5.bw \
	dsp-2-5.dsp \
	dsp-2-5.g \
	dsp-2-5.log \
	dsp-2-5.pix \
	dsp-2-5.rt.pix \
	dsp-3-1.bw \
	dsp-3-1.dsp \
	dsp-3-1.g \
	dsp-3-1.log \
	dsp-3-1.pix \
	dsp-3-1.rt.pix \
	dsp-3-2.bw \
	dsp-3-2.dsp \
	dsp-3-2.g \
	dsp-3-2.log \
	dsp-3-2.pix \
	dsp-3-2.rt.pix \
	dsp-3-3.bw \
	dsp-3-3.dsp \
	dsp-3-3.g \
	dsp-3-3.log \
	dsp-3-3.pix \
	dsp-3-3.rt.pix \
	dsp-3-4.bw \
	dsp-3-4.dsp \
	dsp-3-4.g \
	dsp-3-4.log \
	dsp-3-4.pix \
	dsp-3-4.rt.pix \
	dsp-3-5.bw \
	dsp-3-5.dsp \
	dsp-3-5.g \
	dsp-3-5.log \
	dsp-3-5.pix \
	dsp-3-5.rt.pix \
	dsp-3-6.bw \
	dsp-3-6.dsp \
	dsp-3-6.g \
	dsp-3-6.log \
	dsp-3-6.pix \
	dsp-3-6.rt.pix \
	dsp-3-7.bw \
	dsp-3-7.dsp \
	dsp-3-7.g \
	dsp-3-7.log \
	dsp-3-7.pix \
	dsp-3-7.rt.pix \
	dsp-3-8.bw \
	dsp-3-8.dsp \
	dsp-3-8.g \
	dsp-3-8.log \
	dsp-3-8.pix \
	dsp-3-8.rt.pix \
	eagleCAD-512x438.pix \
	ebm.bw \
	exp_air.pl \
	fastgen_box.fast4 \
	fastgen_dos.g \
	fastgen_unix.g \
	gaps.pl \
	g-dot.log \
	g-nff.err \
	g-nff.log \
	g-nff.m35.asc \
	g-nff.m35.g \
	g-nff.m35.nff \
	gqa.g \
	gqa.mged \
	gqa_mged.log \
	iges.g \
	iges.log \
	iges_file.iges \
	iges_file2.iges \
	iges_file3.iges \
	iges_new.g \
	iges_stdout.iges \
	iges_stdout2.iges \
	iges_stdout_new.g \
	iges.m35.asc \
	iges.m35.g \
	lights.asc \
	lights.g \
	lights.pix \
	lights_diff.pix \
	lights_ref.pix \
	mged.g \
	mged.log \
	moss-diff.log \
	moss.g \
	moss.pix \
	moss.pix.diff \
	moss.png \
	moss2.pix \
	moss_png.diff \
	moss_ref.pix \
	overlaps.pl \
	red.log \
	region_ids \
	regions \
	repository.log \
	shaders.dat \
	shaders.g \
	shaders.mged \
	shaders.pix \
	shaders.pix.diff \
	shaders.rt \
	shaders.rt.diff.pix \
	shaders.rt.log \
	shaders.rt.pix \
	shaders.rt.pixdiff.log \
	shaders.txt \
	shaders_ref.pix \
	solids-diff.log \
	solids-simple.log \
	solids \
	solids.g \
	solids.mged \
	solids.pix.diff \
	solids.rt \
	solids.rt.log \
	solids.rt.pix \
	solids_ref.pix \
	spdi \
	spdi.g \
	spdi.mged \
	spdi.pix \
	spdi_diff.pix \
	spdi_mged.log \
	spdi_ref.pix \
	vdeck.log \
	volume.pl \
	weight.g \
	weight.mged \
	weight.out \
	weight.ref \
	wgt.out

MOSTLYCLEANFILES = \
	fastgen.log \
	flawfinder.log \
	gqa.log \
	lights.log \
	moss-png.log \
	moss.asc \
	moss.log \
	red.log \
	shaders.log \
	solids.log \
	spdi.log \
	weight.log

include $(top_srcdir)/misc/Makefile.defs
