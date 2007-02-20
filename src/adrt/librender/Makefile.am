# $Id$

lib_LTLIBRARIES = librender.la

librender_la_SOURCES = component.c depth.c flat.c grid.c hit.c normal.c path.c plane.c phong.c spall.c render_util.c
librender_la_LDFLAGS = -static
librender_la_LIBADD = ${LIBM}

AM_CFLAGS = -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/libtexture -I${top_srcdir}/src/adrt/libutil

noinst_HEADERS = component.h depth.h flat.h grid.h hit.h normal.h path.h plane.h phong.h render.h render_internal.h spall.h render_util.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
