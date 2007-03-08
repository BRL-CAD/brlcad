# $Id$

# pixtest removed, looks like an outdated sample_application
bin_PROGRAMS = \
	rttherm \
	ssamp-bw \
	ssampview

AM_CPPFLAGS = ${TCL_CPPFLAGS}

dist_bin_SCRIPTS = ssampview.tcl

man_MANS = \
	rttherm.1 \
	ssamp-bw.1

BUILT_SOURCES = vers.c
vers.c: $(ALLSRCS)
	@SH@ $(top_srcdir)/sh/vers.sh version "Thermal Multi-Spectral RT" > $@

rttherm_SOURCES = \
	spectrum.c \
	vers.c \
	viewtherm.c

# have to link static due to librttherm.a, the aix linker, and duplicate symbols in optical/multispectral
rttherm_LDFLAGS = -static ${EXCEPTIONS}
rttherm_CFLAGS = -DRT_MULTISPECTRAL
rttherm_LDADD = \
	$(top_builddir)/src/rt/librttherm.a \
	${MULTISPECTRAL} \
	${OPTICAL} \
	${FB} \
	${SYSV} \
	${PKG} \
	${TCL} 

ssampview_SOURCES = \
	spectrum.c \
	ssampview.c

ssampview_CFLAGS = \
	${X_CFLAGS} \
	${TK_CPPFLAGS}
ssampview_LDADD = \
	${OPTICAL} \
	${FB} \
	${RT} \
	${BN} \
	${BU} \
	${PKG} \
	${SYSV} \
	${TK} \
	${TCL}


ssamp_bw_SOURCES = \
	ssamp-bw.c
ssamp_bw_LDADD = \
	${OPTICAL} \
	${RT} \
	${BN} \
	${BU} \
	${SYSV}

# pixtest removed, looks like an outdated sample_application
#pixtest_SOURCES = \
#	pixtest.c \
#	spectrum.c

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	$(man_MANS) \
	std_day_1km.dat \
	test.sh

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(rttherm_OBJECTS) \
	$(ssamp_bw_OBJECTS) \
	$(ssampview_OBJECTS) \
	$(bin_PROGRAMS)
