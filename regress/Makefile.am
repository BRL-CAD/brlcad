.PHONY : regress moss.pix lights.pix solids.pix shaders.pix spdi.pix

regress: solids.pix shaders.pix spdi.pix moss.pix lights.pix Makefile

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

# these are here because they go in the distribution but are not installed
EXTRA_DIST = solids.sh shaders.sh spdi.sh lights.sh moss.sh \
	solidspix.asc shaderspix.asc spdipix.asc lights_ref.asc mosspix.asc

CLEANFILES = \
	dsp.dat mged_shaders.log mged_solids.log \
	moss.asc moss.g moss.log moss.png moss2.pix moss-png.log moss_png.diff \
	shaders.pix solids solids.g solids.log solids.pix spdi spdi.g spdi_mged.log spdi_ref.pix \
	spdi.log spdi.pix moss.pix moss_ref.pix solids_ref.pix spdi_diff.pix \
	shaders_ref.pix moss-diff.log moss.pix.diff shaders.pix.diff \
	solids-diff.log solids.pix.diff lights.g lights_diff.pix lights_ref.pix lights.pix lights.asc

MOSTLYCLEANFILES = \
	mged_shaders.log mged_solids.log moss.asc moss.log moss-png.log \
	solids.log spdi.log lights.log

MAINTAINERCLEANFILES = Makefile.in

