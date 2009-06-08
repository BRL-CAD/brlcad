
AM_CPPFLAGS = ${TCL_CPPFLAGS}
AM_CFLAGS = \
	-I${top_srcdir}/src/adrt \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libcommon \
	-I${top_srcdir}/src/adrt/librender \
	-I${top_srcdir}/src/adrt/libtexture \
	${TCL_CPPFLAGS}

bin_PROGRAMS = adrt_master adrt_slave
lib_LTLIBRARIES = libtie.la libtexture.la librender.la

libtie_la_LIBADD = libtie0.la libtie1.la ${BU} ${BU_LIBS}
libtie_la_SOURCES =

SOURCES = libtie/tie.c libtie/tie_kdtree.c
libtie0_la_SOURCES = $(SOURCES)
libtie1_la_SOURCES = $(SOURCES)

libtie0_la_CFLAGS = -DTIE_PRECISION=0
libtie1_la_CFLAGS = -DTIE_PRECISION=1

noinst_LTLIBRARIES = libtie0.la libtie1.la

libtexture_la_SOURCES = libtexture/texture_blend.c libtexture/texture_bump.c libtexture/texture_camo.c libtexture/texture_checker.c libtexture/texture_clouds.c libtexture/texture_gradient.c libtexture/texture_image.c libtexture/texture_mix.c libtexture/texture_perlin.c libtexture/texture_stack.c
libtexture_la_LDFLAGS = -static

librender_la_SOURCES = \
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
	librender/surfel.c

librender_la_LIBADD = \
	libtie.la \
	${BN} \
	${BU} \
	${LIBM}

adrt_master_SOURCES = \
	master/compnet.c \
	master/dispatcher.c \
	master/tienet_master.c \
	master/master.c \
	master/main.c

adrt_master_LDADD = \
	libtie.la \
	${BU} \
	${LIBTHREAD} \
	${LIBM} \
	${LIBZ}

adrt_slave_SOURCES = \
	slave/load.c \
	slave/load_MySQL.c \
	slave/load_g.c \
	slave/smain.c \
	slave/slave.c \
	slave/tienet_slave.c

adrt_slave_LDADD = \
	${LIBTHREAD} \
	librender.la \
	${LIBM} \
	libtexture.la \
	libtie.la \
	${LIBZ}

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
	libtexture/texture.h \
	libtexture/texture_blend.h \
	libtexture/texture_bump.h \
	libtexture/texture_camo.h \
	libtexture/texture_checker.h \
	libtexture/texture_clouds.h \
	libtexture/texture_gradient.h \
	libtexture/texture_image.h \
	libtexture/texture_internal.h \
	libtexture/texture_mix.h \
	libtexture/texture_perlin.h \
	libtexture/texture_stack.h \
	libtie/tie.h \
	libtie/tie_define.h \
	libtie/tie_kdtree.h \
	libtie/tie_struct.h

noinst_HEADERS = \
	master/compnet.h \
	master/dispatcher.h \
	master/tienet_master.h \
	master/master.h \
	slave/load.h \
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

EXTRA_DIST = \
	AUTHORS \
	README \
	bench/bench.c  \
	bench/bench.h  \
	bench/main.c \
	doc/ChangeLog \
	doc/binary_spec.txt \
	doc/rise.txt \
	rise/master/dispatcher.c \
	rise/master/dispatcher.h \
	rise/master/main.c \
	rise/master/master.c \
	rise/master/master.h \
	rise/master/post.c \
	rise/master/post.h \
	rise/observer/main.c \
	rise/observer/observer.c \
	rise/observer/observer.h \
	rise/rise.h \
	rise/rise_struct.h \
	rise/slave/main.c \
	rise/slave/slave.c \
	rise/slave/slave.h \
	scripts/adrt.py \
	scripts/drawBSP.py \
	scripts/makemovie.pl \
	scripts/ptcloud.c \
	scripts/ptcloud.py

DEPS = src/libbn

include $(top_srcdir)/misc/Makefile.defs
