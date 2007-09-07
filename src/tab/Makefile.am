# $Id$

bin_PROGRAMS = \
	tabinterp \
	txyz-pl \
	tabsub \
	scriptsort \
	script-tab

AM_CPPFLAGS = ${TCL_CPPFLAGS}
LDADD = ${RT}

scriptsort_SOURCES = scriptsort.c script.l
noinst_HEADERS = tokens.h

man_MANS = scriptsort.1 tabinterp.1 tabsub.1
EXTRA_DIST = $(man_MANS)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(tabinterp_OBJECTS) \
	$(txyz_pl_OBJECTS) \
	$(tabsub_OBJECTS) \
	$(scriptsort_OBJECTS) \
	$(script_tab_OBJECTS) \
	$(bin_PROGRAMS)
