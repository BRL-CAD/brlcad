# $Id$

bin_PROGRAMS = firpass secpass shapefact all_sf display ir-X ir-sgi irdisp

BN=$(top_builddir)/libbn/libbn.la $(BU)
BU=$(top_builddir)/libbu/libbu.la @TCL_LIBS@ @PTHREAD_LIBS@ -lm
FB=$(top_builddir)/libfb/libfb.la @X_LIBS@ $(PKG) $(BU)
PKG=$(top_builddir)/libpkg/libpkg.la
PNG=@PNG@
SYSV=$(top_builddir)/libsysv/libsysv.la
ORLE=$(top_builddir)/liborle/liborle.la
RLE=$(top_builddir)/libutahrle/libutahrle.la -lm
RT=$(top_builddir)/librt/librt.la $(BN)


all_sf_LDADD = ${RT} ${BN} ${BU}
all_sf_SOURCES = all_sf.c

display_LDADD = ${RT} ${BN} ${BU}
display_SOURCES = display.c

firpass_LDADD = ${RT} ${BN} ${BU}
firpass_SOURCES = firpass.c subroutines.c

irdisp_LDADD = ${FB}
irdisp_SOURCES = irdisp.c

ir_sgi_LDADD = ${FB}
ir_sgi_SOURCES = ir-sgi.c

ir_X_LDADD = @X_LIBS@
ir_X_SOURCES = ir-X.c

secpass_LDADD = ${RT} ${BN} ${BU}
secpass_SOURCES = secpass.c subroutines.c

shapefact_LDADD = ${RT} ${BN} ${BU}
shapefact_SOURCES = shapefact.c

lint:
	lint ${LFLAGS} -${I}''${INCDIR} ../irprep/firpass.c ../irprep/subroutines.c > ${SRCDIR}.lint
	lint ${LFLAGS} -${I}''${INCDIR} ../irprep/secpass.c ../irprep/subroutines.c >> ${SRCDIR}.lint
	lint ${LFLAGS} -${I}''${INCDIR} ../irprep/shapefact.c ../irprep/subroutines.c >> ${SRCDIR}.lint
	lint ${LFLAGS} -${I}''${INCDIR} ../irprep/all_sf.c ../irprep/subroutines.c >> ${SRCDIR}.lint
	lint ${LFLAGS} -${I}''${INCDIR} ../irprep/display.c >>${SRCDIR}.lint
	lint ${LFLAGS} -${I}''${INCDIR} ../irprep/ir-X.c >>${SRCDIR}.lint
	lint ${LFLAGS} -${I}''${INCDIR} ../irprep/irdisp.c >>${SRCDIR}.lint


man_MANS = all_sf.1 display.1 firpass.1 irdisp.1 ir-sgi.1 ir-X.1 pictsgi.1 pictx.1 secpass.1 see.1 see2.1 shapefact.1
EXTRA_DIST = $(man_MANS)
