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

# need to test dm-ogl_win.c as a dm-ogl.c replacement
EXTRA_DIST = \
	dm-glx.c \
	dm-ogl_win.c \
	dm-pex.c \
	libdm.dsp

libdm_la_SOURCES = \
	$(SRCS) \
	vers.c

libdm_la_LDFLAGS = -version-info 19:1
libdm_la_CFLAGS = ${X_CFLAGS}
libdm_la_LIBADD = ${TCL} ${TK} ${LIBRT} ${PNG} ${X_LIBS}

AM_CFLAGS = ${X_CFLAGS}

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh dm_version "The BRL-CAD Display Manager Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

