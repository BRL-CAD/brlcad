# $Id$

BU=$(top_builddir)/libbu/libbu.la @TCL_LIBS@ @PTHREAD_LIBS@ -lm 
BN=$(top_builddir)/libbn/libbn.la $(BU)
RT=$(top_builddir)/librt/librt.la $(BN)
FB=$(top_builddir)/libfb/libfb.la @X_LIBS@ $(PKG) $(BU)
PKG=$(top_builddir)/libpkg/libpkg.la
DM=$(top_builddir)/libdm/libdm.la @TK_LIBS@ ${RT}

bin_PROGRAMS = fbserv
fbserv_SOURCES = fbserv.c
fbserv_LDADD = ${FB} ${DM}

man_MANS = fbserv.1
EXTRA_DIST = $(man_MANS)
