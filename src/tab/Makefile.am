
if WITH_PARSERS
SS_BIN = scriptsort
SS_SRC = scriptsort.c script.l
SS_BLT = script.c
else
SS_BIN =
SS_SRC =
SS_BLT =
endif

bin_PROGRAMS = \
	$(SS_BIN) \
	script-tab \
	tabinterp \
	tabsub

AM_CPPFLAGS = \
	${TCL_CPPFLAGS}

LDADD = ${RT}

scriptsort_SOURCES = $(SS_SRC)
noinst_HEADERS = tokens.h

man_MANS = \
	scriptsort.1 \
	tabinterp.1 \
	tabsub.1

BUILT_SOURCES = ${SS_BLT}

DISTCLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	$(man_MANS) \
	script.l \
	scriptsort.c \
	CMakeLists.txt

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(tabinterp_OBJECTS) \
	$(tabsub_OBJECTS) \
	$(scriptsort_OBJECTS) \
	$(script_tab_OBJECTS) \
	$(bin_PROGRAMS)
