
lib_LTLIBRARIES = libtermlib.la

AM_CPPFLAGS = -DCM_N -DCM_GT -DCM_B -DCM_D ${TERMLIB_CPPFLAGS} -DB_TERMCAP="\"${BRLCAD_DATA}/etc/termcap\""

noinst_HEADERS = termcap.h

libtermlib_la_SOURCES = \
	termcap.c \
	tgoto.c \
	tputs.c

termcapdir = $(BRLCAD_DATA)/etc
termcap_DATA = termcap

termcap: termcap.src
	cp $(top_srcdir)/src/other/libtermlib/termcap.src termcap

EXTRA_DIST = \
	termcap \
	termcap.src \
	CMakeLists.txt

BUILT_SOURCES = termcap

DISTCLEANFILES = $(BUILT_SOURCES)

include $(top_srcdir)/misc/Makefile.defs

