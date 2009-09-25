
AM_CPPFLAGS = ${TCL_CPPFLAGS}
AM_CFLAGS = \
	-I${top_srcdir}/src/adrt \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libcommon \
	-I${top_srcdir}/src/adrt/librender \
	-I${top_srcdir}/src/adrt/librender \
	${TCL_CPPFLAGS}

bin_PROGRAMS = adrt_master adrt_slave
lib_LTLIBRARIES = libtie.la librender.la

libtie_la_LIBADD = libtie0.la libtie1.la ${BU} ${BU_LIBS}
libtie_la_SOURCES =

SOURCES = libtie/tie.c libtie/tie_kdtree.c
libtie0_la_SOURCES = $(SOURCES)
libtie1_la_SOURCES = $(SOURCES)

libtie0_la_CFLAGS = -DTIE_PRECISION=0
libtie1_la_CFLAGS = -DTIE_PRECISION=1

noinst_LTLIBRARIES = libtie0.la libtie1.la

librender_la_SOURCES = \
	load.c \
	load_g.c \
	librender/camera.c \
	librender/component.c \
	librender/cut.c \
	librender/depth.c \
	librender/flat.c \
	librender/flos.c \
	librender/grid.c \
	librender/hit.c \
	librender/normal.c \
	librender/path.c \
	librender/phong.c \
	librender/render_util.c \
	librender/spall.c \
	librender/surfel.c \
	librender/texture_blend.c \
	librender/texture_bump.c \
	librender/texture_camo.c \
	librender/texture_checker.c \
	librender/texture_clouds.c \
	librender/texture_gradient.c \
	librender/texture_image.c \
	librender/texture_mix.c \
	librender/texture_perlin.c \
	librender/texture_stack.c

librender_la_LIBADD = \
	libtie.la \
	${BN} \
	${BU} \
	${LIBM}

adrt_master_SOURCES = \
	master/compnet.c \
	master/dispatcher.c \
	master/tienet_master.c \
	master/master.c

adrt_master_LDADD = \
	libtie.la \
	${BU} \
	${LIBTHREAD} \
	${LIBM} \
	${LIBZ}

adrt_slave_SOURCES = \
	slave/slave.c \
	slave/tienet_slave.c

adrt_slave_LDADD = \
	${LIBTHREAD} \
	${LIBZ} \
	${LIBM} \
	librender.la \
	libtie.la \
	${RT} ${GCV}

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = \
	adrt.h  \
	adrt_struct.h \
	librender/camera.h \
	librender/component.h \
	librender/cut.h \
	librender/depth.h \
	librender/flat.h \
	librender/flos.h \
	librender/grid.h \
	librender/hit.h \
	librender/normal.h \
	librender/path.h \
	librender/phong.h \
	librender/render.h \
	librender/render_internal.h \
	librender/render_util.h \
	librender/spall.h \
	librender/surfel.h \
	librender/texture.h \
	librender/texture_internal.h \
	libtie/tie.h \
	libtie/tie_define.h \
	libtie/tie_kdtree.h \
	libtie/tie_struct.h

noinst_HEADERS = \
	load.h \
	master/compnet.h \
	master/dispatcher.h \
	master/tienet_master.h \
	master/master.h \
	slave/slave.h \
	slave/tienet_slave.h

LINT=splint $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES)
lint:
	for file in $(HEADERS) $(SOURCES); do echo $$file; ${LINT} $$file > $${file}.lint; done

clean-local:
	-rm -f $(HEADERS:.h=.h.lint) $(SOURCES:.c=.c.lint)


if HAVE_DOXYGEN
doxygen:
	mkdir doxygen
	doxygen Doxyfile
endif

DEPS = src/libgcv

include $(top_srcdir)/misc/Makefile.defs
