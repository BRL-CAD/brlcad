
bin_PROGRAMS = \
	canonize

AM_CPPFLAGS = ${TCL_CPPFLAGS}
AM_CFLAGS = ${STRICT_FLAGS}

noinst_HEADERS = \
	canon.h \
	chore.h

canonize_SOURCES = canonize.c canonlib.c
canonize_LDADD = ${FB}

EXTRA_DIST = CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(canonize_OBJECTS) \
	$(bin_PROGRAMS)
