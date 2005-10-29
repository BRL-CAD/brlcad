noinst_LTLIBRARIES = libpoints.la

libpoints_la_LDFLAGS = -static
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
	$(cad_boundp_OBJECTS) \
	$(cad_parea_OBJECTS) \
	$(mged_OBJECTS) \
	$(bin_PROGRAMS)
