# $Id$

bin_PROGRAMS = remrt rtsrv

remrt_SOURCES = remrt.c ihost.c vers.c $(top_srcdir)/rt/opt.c
remrt_LDADD = @FB@ @OPTICAL@ @RT@

rtsrv_SOURCES = rtsrv.c $(top_srcdir)/rt/do.c $(top_srcdir)/rt/opt.c $(top_srcdir)/rt/sh_tcl.c $(top_srcdir)/rt/view.c $(top_srcdir)/rt/worker.c
rtsrv_LDADD = @PKG@ @OPTICAL@ @RT@
rtsrv_CFLAGS = -DRTSRV

noinst_HEADERS = ihost.h protocol.h

vers.c:
	$(top_srcdir)/sh/newvers2.sh version "Network-Distributed RT (REMRT)" > vers.c

dist_man_MANS = remrt.1

