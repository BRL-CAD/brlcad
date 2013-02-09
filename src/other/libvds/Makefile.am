
lib_LTLIBRARIES = libvds.la

libvds_la_SOURCES = \
	build.c \
	cluster.c \
	dynamic.c \
	file.c \
	render.c \
	stdfold.c \
	stdvds.c \
	stdvis.c \
	util.c

noinst_HEADERS = \
	vdsprivate.h

include_HEADERS = \
	path.h
	stdvds.h
	vds.h
	vdsprivate.h
	vector.h

EXTRA_DIST = \
	CMakeLists.txt
	COPYING
	README
	docs/fold.png
	docs/intro.html

include $(top_srcdir)/misc/Makefile.defs

