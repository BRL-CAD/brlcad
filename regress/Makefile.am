.PHONY : regression regress test moss.pix lights.pix solids.pix shaders.pix spdi.pix iges

regression: solids.pix shaders.pix spdi.pix moss.pix lights.pix Makefile iges

regress: regression

test: regression

moss.pix: moss.sh
	${SH} moss.sh $(prefix) $(top_srcdir)

lights.pix: lights.sh
	${SH} lights.sh $(prefix) $(top_srcdir)

solids.pix: solids.sh
	${SH} solids.sh $(prefix) $(top_srcdir)

shaders.pix: shaders.sh
	${SH} shaders.sh $(prefix) $(top_srcdir)

spdi.pix: spdi.sh
	${SH} spdi.sh $(prefix) $(top_srcdir)

iges:
	${SH} iges.sh $(prefix) $(top_srcdir)

# these are here because they go in the distribution but are not installed
EXTRA_DIST = \
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
	spdipix.asc

CLEANFILES = \
	dsp.dat \
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
	spdi_ref.pix

MOSTLYCLEANFILES = \
	lights.log \
	mged_shaders.log \
	mged_solids.log \
	moss-png.log \
	moss.asc \
	moss.log \
	solids.log \
	spdi.log

MAINTAINERCLEANFILES = Makefile.in

