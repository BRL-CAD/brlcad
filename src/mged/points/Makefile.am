noinst_LTLIBRARIES = libpoints.la

AM_CPPFLAGS = ${TCL_CPPFLAGS}

libpoints_la_LDFLAGS =
libpoints_la_LIBADD = ${BU}
libpoints_la_SOURCES = \
	count.c \
	points_parse.y \
	points_scan.l \
	process.c \
	main.c

noinst_HEADERS = \
	count.h \
	points_parse.h \
	process.h

points_parse.h: points_parse.y
main.lo: points_parse.h
process.lo: points_parse.h

BUILT_SOURCES = \
	points_parse.c \
	points_parse.h \
	points_scan.c

DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libpoints_la_OBJECTS)
