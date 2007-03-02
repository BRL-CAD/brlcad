# $Id$

lib_LTLIBRARIES = libfb.la

if WITH_OPENGL
IF_OGL_CFLAGS = -DIF_OGL
else
IF_OGL_CFLAGS =
endif

if WITH_WGL
IF_WGL_CFLAGS = -DIF_WGL
else
IF_WGL_CFLAGS =
endif

if WITH_X11
IF_X_CFLAGS = -DIF_X
else
IF_X_CFLAGS =
endif

SRCS = \
	asize.c \
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
	if_wgl.c \
	tcl.c

libfb_la_SOURCES = \
	$(SRCS) \
	vers.c

libfb_la_LDFLAGS = -version-info 19:1
libfb_la_CFLAGS = \
	-DIF_REMOTE \
	-DIF_AB \
	${X_CFLAGS} \
	$(IF_X_CFLAGS) \
	$(IF_OGL_CFLAGS) \
	$(IF_WGL_CFLAGS) \
	${TCL_CPPFLAGS}
libfb_la_LIBADD = \
	${BU} \
	${PKG} \
	${X_LIBS} \
	${LIBGL}

noinst_HEADERS = \
	adage.h \
	adagecursor.h \
	adageframe.h \
	dmdfb.h \
	fblocal.h \
	nilcursor.h \
	oglcursor.h \
	sgicursor.h

dist_man_MANS = libfb.3

# XXX lots of work is needed here to make the other if_ interfaces available.
EXTRA_DIST = \
	fbserv_obj_win32.c \
	if_4d.c \
	if_TEMPLATE.c \
	if_adage.c \
	if_ap.c \
	if_ptty.c \
	if_rat.c \
	if_sgi.c \
	if_sgiw.c \
	if_sun.c \
	if_tk.c \
	if_ts.c \
	if_ug.c \
	libfb.dsp \
	sgiselect.s \
	vers_win.c

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh fb_version "The BRL-CAD Framebuffer Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

DEPS=src/libpkg

include $(top_srcdir)/misc/Makefile.defs
