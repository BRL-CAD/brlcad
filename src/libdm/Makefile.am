# $Id$

lib_LTLIBRARIES = libdm.la

SRCS = \
	axes.c \
	clip.c \
	color.c \
	dm-Null.c \
	dm-X.c \
	dm-generic.c \
	dm-ogl.c \
	dm-plot.c \
	dm-ps.c \
	dm_obj.c \
	knob.c \
	options.c \
	query.c \
	tcl.c 

libdm_la_SOURCES = \
	$(SRCS) \
	vers.c

libdm_la_LDFLAGS = -version-info 19:1
libdm_la_LIBADD = @PNG_LIBS@

AM_CFLAGS = @X_CFLAGS@

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	sh $(top_srcdir)/sh/vers.sh dm_version "The BRL-CAD Display Manager Library" > $@
