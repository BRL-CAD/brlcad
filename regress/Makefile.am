.PHONY : regression regress test moss.pix lights.pix solids.pix shaders.pix spdi.pix iges weight \
	gqa.log

regression: solids.pix shaders.pix spdi.pix moss.pix lights.pix Makefile iges weight \
	gqa.log

regress: regression

test: regression

moss.pix: $(top_srcdir)/regress/moss.sh
	${SH} $(top_srcdir)/regress/moss.sh $(top_srcdir)

lights.pix: $(top_srcdir)/regress/lights.sh
	${SH} $(top_srcdir)/regress/lights.sh $(top_srcdir)

solids.pix: $(top_srcdir)/regress/solids.sh
	${SH} $(top_srcdir)/regress/solids.sh $(top_srcdir)

shaders.pix: $(top_srcdir)/regress/shaders.sh
	${SH} $(top_srcdir)/regress/shaders.sh $(top_srcdir)

spdi.pix: $(top_srcdir)/regress/spdi.sh
	${SH} $(top_srcdir)/regress/spdi.sh $(top_srcdir)

iges:$(top_srcdir)/regress/iges.sh
	${SH} $(top_srcdir)/regress/iges.sh $(top_srcdir)

weight: $(top_srcdir)/regress/weight.sh
	${SH} $(top_srcdir)/regress/weight.sh $(top_srcdir)

gqa.log: $(top_srcdir)/regress/gqa.sh
	${SH} $(top_srcdir)/regress/gqa.sh

# these are here because they go in the distribution but are not installed
EXTRA_DIST = \
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
	dsp.dat \
	exp_air.pl \
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
	mged_shaders.log \
	mged_solids.log \
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
	shaders.pix \
	shaders.pix.diff \
	shaders_ref.pix \
	solids \
	solids-diff.log \
	solids.g \
	solids.log \
	solids.pix \
	solids.pix.diff \
	solids_ref.pix \
	spdi \
	spdi.g \
	spdi.log \
	spdi.pix \
	spdi_diff.pix \
	spdi_mged.log \
	spdi_ref.pix \
	weight.log \
	weight.g \
	weight.out \
	weight.ref \
	wgt.out \
	gqa.g \
	overlaps.pl \
	gaps.pl \
	volume.pl \
	density_table.txt

MOSTLYCLEANFILES = \
	lights.log \
	mged_shaders.log \
	mged_solids.log \
	moss-png.log \
	moss.asc \
	moss.log \
	solids.log \
	spdi.log \
	weight.log \
	gqa.log

MAINTAINERCLEANFILES = Makefile.in


include $(top_srcdir)/misc/Makefile.defs

