# $Id$

bin_PROGRAMS = remrt rtsrv

remrt_LDADD = $(top_builddir)/src/rt/libremrt.a ${FB} ${OPTICAL} ${RT}
remrt_SOURCES = \
	ihost.c \
	remrt.c \
	vers.c

rtsrv_CFLAGS = -DRTSRV
rtsrv_LDADD = $(top_builddir)/src/rt/libremrt.a ${OPTICAL} ${PKG} ${RT}
rtsrv_SOURCES = \
	rtsrv.c \
	vers.c

BUILT_SOURCES = vers.c

vers.c: $(remrt_srcs) Makefile.am
	@SH@ $(top_srcdir)/sh/vers.sh version "Network-Distributed RT (REMRT)" > $@

noinst_HEADERS = ihost.h protocol.h

dist_man_MANS = remrt.1

DISTCLEANFILES = version

MAINTAINERCLEANFILES = Makefile.in
