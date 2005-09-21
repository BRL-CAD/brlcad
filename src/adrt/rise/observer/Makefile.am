# $Id$

bin_PROGRAMS = rise_observer

rise_observer_SOURCES = \
	main.c \
	observer.c

rise_observer_LDFLAGS = -static

rise_observer_CFLAGS = -D__STRICT_ANSI__ @SDL_CFLAGS@ -I.. -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libtexture -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libutil -I${top_srcdir}/src/adrt/rise

rise_observer_LDADD = \
	@SDL_LIBS@ \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${top_builddir}/src/adrt/libtie/libtie.la \
	${top_builddir}/src/adrt/libtexture/libtexture.la \
	${top_builddir}/src/adrt/libcommon/libcommon.la \
	${top_builddir}/src/adrt/librender/librender.la \
	${top_builddir}/src/adrt/libutil/libutil.la \
	${LIBTHREAD} \
	${LIBM} \
	${LIBZ}

noinst_HEADERS = observer.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
