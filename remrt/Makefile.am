# $Id$

bin_PROGRAMS = remrt rtsrv

remrt_srcs = remrt.c ihost.c $(top_srcdir)/rt/opt.c
remrt_SOURCES = $(remrt_srcs) vers.c
remrt_LDADD = @FB@ @OPTICAL@ @RT@

rtsrv_SOURCES = \
	rtsrv.c \
	$(top_srcdir)/rt/do.c \
	$(top_srcdir)/rt/opt.c \
	$(top_srcdir)/rt/sh_tcl.c \
	$(top_srcdir)/rt/view.c \
	$(top_srcdir)/rt/worker.c

rtsrv_LDADD = @PKG@ @OPTICAL@ @RT@
rtsrv_CFLAGS = -DRTSRV

noinst_HEADERS = ihost.h protocol.h

vers.c: $(remrt_srcs) Makefile.am
	sh $(top_srcdir)/sh/vers.sh version "Network-Distributed RT (REMRT)" > $@

dist_man_MANS = remrt.1

