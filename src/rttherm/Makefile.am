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

vers.c: $(ALLSRCS)
	@SH@ $(top_srcdir)/sh/vers.sh version "Thermal Multi-Spectral RT" > $@

rttherm_SOURCES = \
	spectrum.c \
	vers.c \
	viewtherm.c

rttherm_CFLAGS = -DRT_MULTISPECTRAL
rttherm_LDADD = $(top_builddir)/src/rt/librttherm.a $(MULTISPECTRAL) $(LDADD) 

ssampview_SOURCES = \
	spectrum.c \
	ssampview.c

ssampview_CFLAGS = @X_CFLAGS@
ssampview_LDADD = \
	$(TK) \
	$(LDADD)

ssamp_bw_SOURCES = \
	ssamp-bw.c

# pixtest removed, looks like an outdated sample_application
#pixtest_SOURCES = \
#	pixtest.c \
#	spectrum.c

BUILT_SOURCES = vers.c

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	$(man_MANS) \
	std_day_1km.dat \
	test.sh

DISTCLEANFILES = \
	$(BUILT_SOURCES) \
	version

MAINTAINERCLEANFILES = Makefile.in
