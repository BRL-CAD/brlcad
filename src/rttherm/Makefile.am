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

rttherm_SOURCES = \
	spectrum.c \
	viewtherm.c

# have to link static due to librttherm.a, the aix linker, and duplicate symbols in optical/multispectral
rttherm_LDFLAGS = -static
rttherm_CFLAGS = -DRT_MULTISPECTRAL
rttherm_LDADD = \
	$(top_builddir)/src/rt/librttherm.a \
	${MULTISPECTRAL} ${MULTISPECTRAL_LIBS} \
	${OPTICAL} ${OPTICAL_LIBS} \
	${PKG} ${PKG_LIBS} \
	${RT} ${RT_LIBS} \
	${FB} ${FB_LIBS} \
	${SYSV} ${SYSV_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${TCL} 

ssampview_SOURCES = \
	spectrum.c \
	ssampview.c

ssampview_CFLAGS = \
	${X_CFLAGS} \
	${TK_CPPFLAGS}
ssampview_LDADD = \
	${OPTICAL} ${OPTICAL_LIBS} \
	${FB} ${FB_LIBS} \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${PKG} ${PKG_LIBS} \
	${SYSV} ${SYSV_LIBS} \
	${TK} \
	${TCL}


ssamp_bw_SOURCES = \
	ssamp-bw.c
ssamp_bw_LDADD = \
	${OPTICAL} ${OPTICAL_LIBS} \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

# pixtest removed, looks like an outdated sample_application
#pixtest_SOURCES = \
#	pixtest.c \
#	spectrum.c

EXTRA_DIST = \
	$(man_MANS) \
	std_day_1km.dat \
	test.sh

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(rttherm_OBJECTS) \
	$(ssamp_bw_OBJECTS) \
	$(ssampview_OBJECTS) \
	$(bin_PROGRAMS)
