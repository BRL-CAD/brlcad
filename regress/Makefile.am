regress: solids.pix shaders.pix spdi.pix moss.pix

moss.pix: solids.sh
	${SH} solids.sh $(prefix) $(top_srcdir)

solids.pix: solids.sh
	${SH} solids.sh $(prefix) $(top_srcdir)

shaders.pix: shaders.sh
	${SH} shaders.sh $(prefix) $(top_srcdir)

spdi.pix: spdi.sh
	${SH} spdi.sh $(prefix) $(top_srcdir)

CLEANFILES = \
	dsp.dat mged_shaders.log mged_solids.log moss.asc moss.g moss.log \
	shaders.pix solids solids.g solids.log solids.pix spdi spdi.g \
	spdi.log spdi.pix moss.pix

MOSTLYCLEANFILES = \
	mged_shaders.log mged_solids.log moss.asc moss.log \
	solids.log spdi.log

MAINTAINERCLEANFILES = Makefile.in