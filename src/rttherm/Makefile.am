# $Id$

common_files = \
	ssampview.c \
	viewtherm.c

bin_PROGRAMS = \
	pixtest \
	rttherm \
	ssamp-bw \
	ssampview

dist_bin_SCRIPTS = ssampview.tcl

LDADD = ${RT} ${FB} ${OPTICAL} ${TCL}

man_MANS = \
	rttherm.1 \
	ssamp-bw.1

EXTRA_DIST = \
	std_day_1km.dat
	test.sh

rttherm_SOURCES = \
	$(common_files) \
	../material.c \
	../rt/do.c \
	../rt/main.c \
	../rt/opt.c \
	../rt/worker.c \
	../rt/wray.c \
	spectrum.c \
	vers.c

ssampview_SOURCES = \
	spectrum.c \
	ssampview.c

ssamp_bw_SOURCES = \
	ssamp-bw.c

pixtest_SOURCES = \
	pixtest.c \
	spectrum.c

BUILT_SOURCES = vers.c
vers.c: $(ALLSRCS)
	@SH@ $(top_srcdir)/sh/vers.sh version "Thermal Multi-Spectral RT" > $@
