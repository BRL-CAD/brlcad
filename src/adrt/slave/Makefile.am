
bin_PROGRAMS = adrt_slave

adrt_slave_SOURCES = \
	load.c \
	load_MySQL.c \
	load_g.c \
	main.c \
	slave.c \
	tienet_slave.c

adrt_slave_CFLAGS = \
	-I${top_srcdir}/src/adrt \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtexture \
	-I${top_srcdir}/src/adrt/librender \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS}

adrt_slave_LDADD = \
	${LIBTHREAD} \
	${top_builddir}/src/adrt/librender/librender.la \
	${LIBM} \
	${top_builddir}/src/adrt/libtexture/libtexture.la \
	${top_builddir}/src/adrt/libtie/libtie.la \
	${LIBZ}

noinst_HEADERS = \
	load.h \
	slave.h \
	tienet_slave.h

EXTRA_DIST = g-adrt.c

include $(top_srcdir)/misc/Makefile.defs
