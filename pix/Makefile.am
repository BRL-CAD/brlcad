
BENCHMARK_PIX = \
	bldg391.pix \
	m35.pix \
	moss.pix \
	sphflake.pix \
	star.pix \
	world.pix


PIX = \
	${BENCHMARK_PIX} \
	golf_ball.pix \
	lgt1.pix \
	lgt2.pix \
	lgt3.pix \
	mworld.pix \
	toilet.pix

LOG = \
	bldg391.log \
	m35.log \
	moss.log \
	sphflake.log \
	star.log \
	world.log

SAVEVIEWS = \
	cube.rt

EXTRA_DIST = \
	${BENCHMARK_PIX} \
	${PIX} \
	${LOG} \
	${SAVEVIEWS} \
	CMakeLists.txt

pixdir = $(BRLCAD_DATA)/pix
pix_DATA = \
	${BENCHMARK_PIX} \
	${LOG}

include $(top_srcdir)/misc/Makefile.defs
