
bin_PROGRAMS = remrt rtsrv

AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

dist_man_MANS = remrt.1

remrt_LDADD = \
	$(top_builddir)/src/rt/libremrt.a \
	${OPTICAL} \
	${FB}
remrt_SOURCES = \
	ihost.c \
	remrt.c

rtsrv_CFLAGS = -DRTSRV
rtsrv_LDADD = \
	$(top_builddir)/src/rt/libremrt.a \
	${OPTICAL} \
	${PKG} \
	${LIBSOCKET} ${LIBNSL}

rtsrv_SOURCES = \
	rtsrv.c

noinst_HEADERS = ihost.h protocol.h

EXTRA_DIST = CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(remrt_OBJECTS) \
	$(rtsrv_OBJECTS) \
	$(bin_PROGRAMS)
