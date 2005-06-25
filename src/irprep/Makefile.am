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

all_sf_LDADD = ${LIBRT} ${BN}
all_sf_SOURCES = all_sf.c

showtherm_LDADD = ${LIBRT} ${BN}
showtherm_SOURCES = showtherm.c

firpass_LDADD = ${LIBRT} ${BN}
firpass_SOURCES = firpass.c subroutines.c

irdisp_LDADD = ${FB}
irdisp_SOURCES = irdisp.c
irdisp_CFLAGS = ${X_CFLAGS}

ir_sgi_LDADD = ${FB}
ir_sgi_SOURCES = ir-sgi.c

ir_X_LDADD = ${X_LIBS}
ir_X_SOURCES = ir-X.c
ir_X_CFLAGS = ${X_CFLAGS}

secpass_LDADD = ${LIBRT} ${BN}
secpass_SOURCES = secpass.c subroutines.c

shapefact_LDADD = ${LIBRT} ${BN}
shapefact_SOURCES = shapefact.c

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
