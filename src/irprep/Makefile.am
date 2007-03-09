# $Id$

bin_PROGRAMS = \
	all_sf \
	showtherm \
	firpass \
	ir-X \
	ir-sgi \
	irdisp \
	secpass \
	shapefact

AM_CPPFLAGS = ${TCL_CPPFLAGS}

all_sf_SOURCES = all_sf.c
all_sf_LDADD = \
	${RT} \
	${BN} \
	${BU} \
	${SYSV}

showtherm_SOURCES = showtherm.c
showtherm_LDADD = \
	${RT} \
	${BN} \
	${BU} \
	${SYSV}

firpass_SOURCES = firpass.c subroutines.c
firpass_LDADD = \
	${RT} \
	${BN} \
	${BU} \
	${SYSV}

irdisp_SOURCES = irdisp.c
irdisp_CFLAGS = ${X_CFLAGS}

ir_sgi_SOURCES = ir-sgi.c
ir_sgi_LDADD = \
	${BU}

ir_X_SOURCES = ir-X.c
ir_X_CFLAGS = ${X_CFLAGS}
ir_X_LDADD = \
	${BU} \
	${X_LIBS}

secpass_SOURCES = secpass.c subroutines.c
secpass_LDADD = \
	${RT} \
	${BN} \
	${BU} \
	${SYSV}

shapefact_SOURCES = shapefact.c
shapefact_LDADD = \
	${RT} \
	${BN} \
	${BU} \
	${SYSV}

man_MANS = \
	all_sf.1 \
	showtherm.1 \
	firpass.1 \
	irdisp.1 \
	ir-sgi.1 \
	ir-X.1 \
	secpass.1 \
	shapefact.1

EXTRA_DIST = \
	$(man_MANS) \
	Compile.sgi \
	pictsgi.1 \
	pictsgi.c \
	pictx.1 \
	pictx.c \
	see.1 \
	see.c \
	see2.1 \
	see2.c

DEPS=src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(all_sf_OBJECTS) \
	$(showtherm_OBJECTS) \
	$(firpass_OBJECTS) \
	$(ir_X_OBJECTS) \
	$(ir_sgi_OBJECTS) \
	$(irdisp_OBJECTS) \
	$(secpass_OBJECTS) \
	$(shapefact_OBJECTS) \
	$(bin_PROGRAMS)
