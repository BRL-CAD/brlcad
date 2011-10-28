
bin_PROGRAMS = \
	canonize

AM_CPPFLAGS = ${TCL_CPPFLAGS}
AM_CFLAGS = ${STRICT_FLAGS}

noinst_HEADERS = \
	canon.h \
	chore.h

man_MANS = canonize.1

canonize_SOURCES = canonize.c canonlib.c
canonize_LDADD = ${FB}

EXTRA_DIST = $(man_MANS) CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(canonize_OBJECTS) \
	$(bin_PROGRAMS)
