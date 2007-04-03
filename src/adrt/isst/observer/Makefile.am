# $Id$

bin_PROGRAMS = isst_observer

isst_observer_SOURCES = \
	main.c \
	observer.c

isst_observer_CFLAGS = \
	-D__STRICT_ANSI__ \
	@SDL_CFLAGS@ \
	-I.. \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtienet \
	-I${top_srcdir}/src/adrt/libtexture \
	-I${top_srcdir}/src/adrt/libcommon \
	-I${top_srcdir}/src/adrt/librender \
	-I${top_srcdir}/src/adrt/libutil \
	-I${top_srcdir}/src/adrt/isst \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS}

# LDADD = @BRLCAD_LDFLAGS@ @SDL_LIBS@ ${top_builddir}/libtie/.libs/libtie.a ${top_builddir}/libtienet/.libs/libtienet.a ${top_builddir}/libtexture/.libs/libtexture.a ${top_builddir}/libcommon/.libs/libcommon.a ${top_builddir}/librender/.libs/librender.a -lpthread

isst_observer_LDADD = \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${top_builddir}/src/adrt/libtie/libtie.la \
	${top_builddir}/src/adrt/libtexture/libtexture.la \
	${top_builddir}/src/adrt/libcommon/libcommon.la \
	${top_builddir}/src/adrt/librender/librender.la \
	${top_builddir}/src/adrt/libutil/libutil.la \
	${SDL_LIBS} \
	${LIBTHREAD} \
	${LIBZ} \
	${LIBM}

isst_observer_LDFLAGS = -static
noinst_HEADERS = \
	observer.h \
	splash.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
