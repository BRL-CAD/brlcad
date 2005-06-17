# $Id$

lib_LTLIBRARIES = librender.la

librender_la_SOURCES = component.c flat.c kelos.c grid.c hit.c normal.c path.c plane.c phong.c shotline.c

AM_CFLAGS = -I${top_srcdir}/libtie -I${top_srcdir}/libcommon -I${top_srcdir}/libtexture -I${top_srcdir}/libutil -lm

noinst_HEADERS = component.h flat.h kelos.h grid.h hit.h normal.h path.h plane.h phong.h render.h render_internal.h shotline.h
