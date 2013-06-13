
AM_CPPFLAGS = ${TCL_CPPFLAGS}
AM_CFLAGS = \
	-I${top_srcdir}/src/adrt \
	-I${top_srcdir}/src/adrt/libcommon \
	-I${top_srcdir}/src/adrt/librender \
	-I${top_srcdir}/src/adrt/librender \
	${TCL_CPPFLAGS}

lib_LTLIBRARIES = librender.la

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
	${GCV} \
	${GCV_LIBS}

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = \
	adrt.h  \
	adrt_struct.h \
	librender/camera.h \
	librender/render.h \
	librender/render_internal.h \
	librender/render_util.h \
	librender/texture.h \
	librender/texture_internal.h

noinst_HEADERS = \
	load.h \
	tienet.h

pkglibdir = $(libdir)/isst

EXTRA_DIST = \
	isst \
	isst.bat \
	isst.h \
	isst_tcltk.c \
	master/compnet.c \
	master/compnet.h \
	master/dispatcher.c \
	master/dispatcher.h \
	master/master.c \
	master/master.h \
	master/tienet_master.c \
	master/tienet_master.h \
	slave/slave.c \
	slave/slave.h \
	slave/tienet_slave.c \
	slave/tienet_slave.h \
	CMakeLists.txt

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
