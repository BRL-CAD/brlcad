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

libdm_la_SOURCES = \
	axes.c \
	clip.c \
	color.c \
	dm-Null.c \
	dm-X.c \
	dm-generic.c \
	dm-ogl.c \
	dm-plot.c \
	dm-ps.c \
	dm-wgl.c \
	dm_obj.c \
	knob.c \
	options.c \
	query.c \
	tcl.c \
	vers.c

libdm_la_LDFLAGS = -version-info 19:1
# per-program CPPFLAGS wasn't added until automake 1.7
libdm_la_CFLAGS = \
	${X_CFLAGS} \
	${DM_X_CFLAGS} \
	${DM_OGL_CFLAGS} \
	${DM_WGL_CFLAGS} \
	${PNG_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS}

EXTRA_DIST = \
	dm-glx.c \
	dm-pex.c \
	dm-tk.c \
	libdm.dsp

DEPS=src/librt

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
