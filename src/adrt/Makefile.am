
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

#if BUILD_TOGL
#pkglib_LTLIBRARIES = issttcltk.la
#
#issttcltk_la_SOURCES = isst_tcltk.c
#
#issttcltk_la_DEPENDENCIES = libtie.la librender.la
#
#issttcltk_la_LIBADD = ${TIE} ${RENDER} ${TCL} ${TK} -L$(top_builddir)/src/other/togl -lToglstub2.1
#issttcltk_la_CFLAGS = -I$(top_srcdir)/src/adrt/libtie -I$(top_srcdir)/src/adrt/librender  -DTIE_PRECISION=0 ${TCL_CPPFLAGS} ${TK_CPPFLAGS} -I$(top_srcdir)/src/other/togl -I$(top_builddir)/src/other/togl ${GL_CPPFLAGS}
#issttcltk_la_LDFLAGS = -module -shrext .plugin
#
#pkgIndex.tcl:
#	@echo 'package ifneeded isst 0.1 [list load [file join $(pkglibdir) issttcltk.plugin] isst]' > pkgIndex.tcl
#
#issttcltkscriptdir = $(pkglibdir)
#
#issttcltkscript_SCRIPTS = pkgIndex.tcl
#endif

EXTRA_DIST = \
	isst.h \
	isst \
	isst_tcltk.c \
	master/compnet.h \
	master/master.h \
	master/tienet_master.c \
	master/dispatcher.c \
	master/master.c \
	master/dispatcher.h \
	master/tienet_master.h \
	master/compnet.c \
	slave/tienet_slave.h \
	slave/slave.h \
	slave/slave.c \
	slave/tienet_slave.c \
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
