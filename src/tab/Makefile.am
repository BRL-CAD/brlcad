# $Id$

bin_PROGRAMS = \
	tabinterp \
	txyz-pl \
	tabsub \
	scriptsort \
	script-tab

LDADD = ${LIBRT}
scriptsort_SOURCES = scriptsort.c script.l
noinst_HEADERS = tokens.h

man_MANS = scriptsort.1 tabinterp.1 tabsub.1
EXTRA_DIST = $(man_MANS)

