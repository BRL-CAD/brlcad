# $Id$

common_files = \
	ssampview.c \
	viewtherm.c

# pixtest removed, looks like an outdated sample_application
bin_PROGRAMS = \
	rttherm \
	ssamp-bw \
	ssampview

dist_bin_SCRIPTS = ssampview.tcl

LDADD = ${RT} ${FB} ${OPTICAL} ${TCL}

man_MANS = \
	rttherm.1 \
	ssamp-bw.1

EXTRA_DIST = \
	std_day_1km.dat \
	test.sh \
	$(man_MANS)



do.c: $(top_srcdir)/src/rt/do.c
	$(CP) $(top_srcdir)/src/rt/do.c .

ext.h: $(top_srcdir)/src/rt/ext.h
	$(CP) $(top_srcdir)/src/rt/ext.h .

main.c: $(top_srcdir)/src/rt/main.c
	$(CP) $(top_srcdir)/src/rt/main.c .

opt.c: $(top_srcdir)/src/rt/opt.c
	$(CP) $(top_srcdir)/src/rt/opt.c .

worker.c: $(top_srcdir)/src/rt/worker.c
	$(CP) $(top_srcdir)/src/rt/worker.c .

vers.c: $(ALLSRCS)
	@SH@ $(top_srcdir)/sh/vers.sh version "Thermal Multi-Spectral RT" > $@

noinst_LTLIBRARIES = librttherm.la

librttherm_la_SOURCES = \
	do.c \
	main.c \
	ext.h \
	opt.c \
	worker.c

rttherm_SOURCES = \
	spectrum.c \
	vers.c \
	viewtherm.c

rttherm_CFLAGS = -DRT_MULTISPECTRAL
rttherm_LDADD = $(MULTISPECTRAL) $(LDADD) librttherm.la

ssampview_SOURCES = \
	spectrum.c \
	ssampview.c

ssampview_LDADD = \
	$(TK) \
	$(LDADD)

ssamp_bw_SOURCES = \
	ssamp-bw.c

#pixtest_SOURCES = \
#	pixtest.c \
#	spectrum.c

BUILT_SOURCES = \
	$(librttherm_la_SOURCES) \
	vers.c

DISTCLEANFILES = $(BUILT_SOURCES)
