noinst_LTLIBRARIES = libpoints.la

libpoints_la_LDFLAGS =
libpoints_la_LIBADD = ${BU}
libpoints_la_SOURCES = \
	count.c \
	main.c \
	points_parse.y \
	points_scan.l \
	process.c

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libpoints_la_OBJECTS)
