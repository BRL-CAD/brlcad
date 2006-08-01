# $Id$

lib_LTLIBRARIES = libdm.la

if WITH_OPENGL
DM_OGL_CFLAGS = -DDM_OGL -DIF_OGL
else
DM_OGL_CFLAGS =
endif

if WITH_WGL
DM_WGL_CFLAGS = -DDM_WGL -DIF_WGL
else
DM_WGL_CFLAGS =
endif

if WITH_X11
DM_X_CFLAGS = -DDM_X -DIF_X
else
DM_X_CFLAGS =
endif

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
	dm-wgl.c \
	knob.c \
	options.c \
	query.c \
	tcl.c

libdm_la_SOURCES = \
	$(SRCS) \
	vers.c

libdm_la_LDFLAGS = -version-info 19:1
libdm_la_CFLAGS = ${X_CFLAGS} ${DM_X_CFLAGS} ${DM_OGL_CFLAGS} ${DM_WGL_CFLAGS}
libdm_la_LIBADD = \
	${TCL} \
	${RT} \
	${PNG} \
	${X_LIBS}

EXTRA_DIST = \
	dm-glx.c \
	dm-pex.c \
	libdm.dsp \
	vers_win.c

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh dm_version "The BRL-CAD Display Manager Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

