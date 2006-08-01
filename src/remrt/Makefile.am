# $Id$

bin_PROGRAMS = remrt rtsrv

dist_man_MANS = remrt.1

remrt_LDADD = \
	$(top_builddir)/src/rt/libremrt.a \
	${FB} \
	${OPTICAL} \
	${RT}
remrt_SOURCES = \
	ihost.c \
	remrt.c \
	vers.c

rtsrv_CFLAGS = -DRTSRV
rtsrv_LDADD = \
	$(top_builddir)/src/rt/libremrt.a \
	${OPTICAL} \
	${PKG} \
	${RT}
rtsrv_SOURCES = \
	rtsrv.c \
	vers.c

BUILT_SOURCES = vers.c
vers.c: $(remrt_srcs) Makefile.am
	@SH@ $(top_srcdir)/sh/vers.sh version "Network-Distributed RT (REMRT)" > $@

noinst_HEADERS = ihost.h protocol.h

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(remrt_OBJECTS) \
	$(rtsrv_OBJECTS) \
	$(bin_PROGRAMS)
