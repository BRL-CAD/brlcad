# $Id$

bin_PROGRAMS = remrt rtsrv

remrt_srcs = \
	opt.c \
	ihost.c \
	remrt.c

remrt_SOURCES = $(remrt_srcs) vers.c
remrt_LDADD = ${FB} ${OPTICAL} ${RT}

rtsrv_SOURCES = \
	do.c \
	ext.h \
	opt.c \
	rtsrv.c \
	sh_tcl.c \
	view.c \
	worker.c

rtsrv_LDADD = \
	${OPTICAL} \
	${PKG} \
	${RT}

do.c: $(top_srcdir)/src/rt/do.c
	$(CP) $(top_srcdir)/src/rt/do.c .

ext.h: $(top_srcdir)/src/rt/ext.h
	$(CP) $(top_srcdir)/src/rt/ext.h .

opt.c: $(top_srcdir)/src/rt/opt.c
	$(CP) $(top_srcdir)/src/rt/opt.c .

sh_tcl.c: $(top_srcdir)/src/rt/sh_tcl.c
	$(CP) $(top_srcdir)/src/rt/sh_tcl.c .

view.c: $(top_srcdir)/src/rt/view.c
	$(CP) $(top_srcdir)/src/rt/view.c .

worker.c: $(top_srcdir)/src/rt/worker.c
	$(CP) $(top_srcdir)/src/rt/worker.c .

vers.c: $(remrt_srcs) Makefile.am
	@SH@ $(top_srcdir)/sh/vers.sh version "Network-Distributed RT (REMRT)" > $@

rtsrv_CFLAGS = -DRTSRV

noinst_HEADERS = ihost.h protocol.h

dist_man_MANS = remrt.1

BUILT_SOURCES = \
	do.c \
	ext.h \
	opt.c \
	sh_tcl.c \
	view.c \
	worker.c \
	vers.c

DISTCLEANFILES = $(BUILT_SOURCES)
