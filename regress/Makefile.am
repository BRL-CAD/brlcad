regress: solids.pix shaders.pix spdi.pix moss.pix lights.pix

moss.pix: solids.sh
	${SH} solids.sh $(prefix) $(top_srcdir)

lights.pix: lights.sh
	${SH} lights.sh $(prefix) $(top_srcdir)

solids.pix: solids.sh
	${SH} solids.sh $(prefix) $(top_srcdir)

shaders.pix: shaders.sh
	${SH} shaders.sh $(prefix) $(top_srcdir)

spdi.pix: spdi.sh
	${SH} spdi.sh $(prefix) $(top_srcdir)

# these are here because they go in the distribution but are not installed
EXTRA_DIST = solids.sh shaders.sh spdi.sh

CLEANFILES = \
	dsp.dat mged_shaders.log mged_solids.log moss.asc moss.g moss.log \
	shaders.pix solids solids.g solids.log solids.pix spdi spdi.g \
	spdi.log spdi.pix moss.pix moss_ref.pix solids_ref.pix \
	shaders_ref.pix moss-diff.log moss.pix.diff shaders.pix.diff \
	solids-diff.log solids.pix.diff lights.g lights_diff.pix lights_ref.pix lights.pix

MOSTLYCLEANFILES = \
	mged_shaders.log mged_solids.log moss.asc moss.log \
	solids.log spdi.log lights.log

MAINTAINERCLEANFILES = Makefile.in

