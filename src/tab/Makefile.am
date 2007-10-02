# $Id$

if WITH_PARSERS
SS_BIN = scriptsort
SS_SRC = scriptsort.c script.l
else
SS_BIN =
SS_SRC =
endif

bin_PROGRAMS = \
	$(SS_BIN) \
	script-tab \
	tabinterp \
	tabsub \
	txyz-pl

AM_CPPFLAGS = ${TCL_CPPFLAGS}
LDADD = ${RT}

scriptsort_SOURCES = $(SS_SRC)
noinst_HEADERS = tokens.h

man_MANS = \
	scriptsort.1 \
	tabinterp.1 \
	tabsub.1

EXTRA_DIST = \
	$(man_MANS) \
	scriptsort.c \
	script.l

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(tabinterp_OBJECTS) \
	$(txyz_pl_OBJECTS) \
	$(tabsub_OBJECTS) \
	$(scriptsort_OBJECTS) \
	$(script_tab_OBJECTS) \
	$(bin_PROGRAMS)
