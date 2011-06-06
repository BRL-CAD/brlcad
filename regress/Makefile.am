.PHONY : regression regress test mged red moss lights solids shaders spdi iges weight gqa fastgen bots flawfinder repository

SUBDIRS = mged

regression: clean mged moss lights solids shaders spdi iges weight gqa fastgen bots flawfinder repository
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

flawfinder: $(top_srcdir)/regress/flawfinder.sh
	-${SH} $(top_srcdir)/regress/flawfinder.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

repository: $(top_srcdir)/regress/repository.sh
	-${SH} $(top_srcdir)/regress/repository.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

# these are here because they go in the distribution but are not installed
EXTRA_DIST = \
	bots.sh \
	fastgen.sh \
	fastgen_dos.fast4 \
	flawfinder.sh \
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
	weight.sh \
	CMakeLists.txt

CLEANFILES = \
	$(MOSTLYCLEANFILES) \
	.density \
	adj_air.pl \
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
	density_table.txt \
	dsp.dat \
	eagleCAD-512x438.pix \
	ebm.bw \
	exp_air.pl \
	fastgen_box.fast4 \
	fastgen_dos.g \
	fastgen_unix.g \
	gaps.pl \
	gqa.g \
	gqa.mged \
	gqa_mged.log \
	iges.g \
	iges.log \
	iges_file.iges \
	iges_new.g \
	iges_stdout.iges \
	iges_stdout_new.g \
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
	volume.pl \
	weight.g \
	weight.mged \
	weight.out \
	weight.ref \
	wgt.out \
	CMakeLists.txt

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
