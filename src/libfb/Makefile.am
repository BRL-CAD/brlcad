# $Id$

lib_LTLIBRARIES = libfb.la

SRCS = \
	fb_generic.c \
	fb_log.c \
	fb_obj.c \
	fb_paged_io.c \
	fb_rect.c \
	fb_util.c \
	fbserv_obj.c \
	getput.c \
	if_X.c \
	if_X24.c \
	if_ab.c \
	if_debug.c \
	if_disk.c \
	if_mem.c \
	if_null.c \
	if_ogl.c \
	if_remote.c \
	if_stack.c \
	server.c \
	tcl.c

libfb_la_SOURCES = \
	$(SRCS) \
	vers.c

libfb_la_LDFLAGS = -version-info 19:1

include_HEADERS = \
	adage.h \
	adagecursor.h \
	adageframe.h \
	dmdfb.h \
	fblocal.h \
	nilcursor.h \
	oglcursor.h \
	pkgtypes.h \
	sgicursor.h

dist_man_MANS = libfb.3

AM_CFLAGS = @X_CFLAGS@ -DIF_REMOTE -DIF_AB -DIF_X -DIF_OGL

libfb_la_LIBADD = @PKG@ @X_LIBS@

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh fb_version "The BRL-CAD Framebuffer Library" > $@

