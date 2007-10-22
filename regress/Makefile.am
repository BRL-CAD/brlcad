.PHONY : regression regress test moss lights solids shaders spdi iges weight gqa

regression: clean solids shaders spdi moss lights Makefile iges weight gqa flawfinder
	@${ECHO} ---
	@${ECHO} Regression testing completed.

regress: regression

test: regression

moss: $(top_srcdir)/regress/moss.sh
	-${SH} $(top_srcdir)/regress/moss.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

lights: $(top_srcdir)/regress/lights.sh
	-${SH} $(top_srcdir)/regress/lights.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

solids: $(top_srcdir)/regress/solids.sh
	-${SH} $(top_srcdir)/regress/solids.sh $(top_srcdir)
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
	-${SH} $(top_srcdir)/regress/gqa.sh
	@${ECHO} +++ $@ test complete.

flawfinder: $(top_srcdir)/regress/flawfinder.sh
	-${SH} $(top_srcdir)/regress/flawfinder.sh $(top_srcdir)
	@${ECHO} +++ $@ test complete.

# these are here because they go in the distribution but are not installed
EXTRA_DIST = \
	flawfinder.sh \
	gqa.sh \
	iges.sh \
	lights.sh \
	lights_ref.asc \
	moss.sh \
	mosspix.asc \
	shaders.sh \
	shaderspix.asc \
	solids.sh \
	solidspix.asc \
	spdi.sh \
	spdipix.asc \
	weight.sh

CLEANFILES = \
	.density \
	adj_air.pl \
	density_table.txt \
	dsp.dat \
	eagleCAD-512x438.pix \
	ebm.bw \
	exp_air.pl \
	flawfinder.log \
	gaps.pl \
	gqa.g \
	gqa.log \
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
	moss-diff.log \
	moss-png.log \
	moss.asc \
	moss.g \
	moss.log \
	moss.pix \
	moss.pix.diff \
	moss.png \
	moss2.pix \
	moss_png.diff \
	moss_ref.pix \
	overlaps.pl \
	shaders.dat \
	shaders.g \
	shaders.log \
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
	solids.log \
	solids.mged \
	solids.pix.diff \
	solids.rt \
	solids.rt.pix \
	solids_ref.pix \
	spdi \
	spdi.g \
	spdi.log \
	spdi.mged \
	spdi.pix \
	spdi_diff.pix \
	spdi_mged.log \
	spdi_ref.pix \
	volume.pl \
	weight.g \
	weight.log \
	weight.mged \
	weight.out \
	weight.ref \
	wgt.out

MOSTLYCLEANFILES = \
	flawfinder.log \
	gqa.log \
	lights.log \
	moss-png.log \
	moss.asc \
	moss.log \
	shaders.log \
	solids.log \
	spdi.log \
	weight.log

include $(top_srcdir)/misc/Makefile.defs
