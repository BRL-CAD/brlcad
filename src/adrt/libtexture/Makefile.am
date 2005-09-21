# $Id$

lib_LTLIBRARIES = libtexture.la

libtexture_la_SOURCES = texture_blend.c texture_bump.c texture_camo.c texture_checker.c texture_clouds.c texture_gradient.c texture_image.c texture_mix.c texture_perlin.c texture_stack.c
libtexture_la_LDFLAGS = -static

AM_CFLAGS = -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libutil

noinst_HEADERS = texture_blend.h texture_bump.h texture_camo.h texture_checker.h texture_clouds.h texture_gradient.h texture.h texture_image.h texture_internal.h texture_mix.h texture_perlin.h texture_stack.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
