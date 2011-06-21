
lib_LTLIBRARIES = libfb.la

if WITH_WGL
IF_WGL_CFLAGS = -DIF_WGL
else
IF_WGL_CFLAGS =
endif

if WITH_X11
IF_X_CFLAGS = -DIF_X ${X_CFLAGS}
IF_X_LIBADD = ${X_LIBS}
else
IF_X_CFLAGS =
IF_X_LIBADD =
endif

if WITH_OPENGL
IF_OGL_CFLAGS = -DIF_OGL
IF_OGL_LIBADD = ${LIBGL}
else
IF_OGL_CFLAGS =
IF_OGL_LIBADD =
endif

if WITH_TK
IF_TK_CFLAGS = -DIF_TK $(TK_CPPFLAGS)
IF_TK_LIBADD = ${TK}
else
IF_TK_CFLAGS =
IF_TK_LIBADD =
endif

libfb_la_SOURCES = \
	asize.c \
	fb_generic.c \
	fb_log.c \
	fb_obj.c \
	fb_paged_io.c \
	fb_rect.c \
	fb_util.c \
	fbserv_obj.c \
	if_X.c \
	if_X24.c \
	if_debug.c \
	if_disk.c \
	if_mem.c \
	if_null.c \
	if_ogl.c \
	if_remote.c \
	if_stack.c \
	if_tk.c \
	if_wgl.c \
	tcl.c \
	vers.c

libfb_la_LDFLAGS = -version-info 19:1 -no-undefined

# STD_C99_FLAG is needed due to gl.h on Mac OS X 10.6 having // comments
libfb_la_CFLAGS = \
	${STD_C99_FLAGS} \
	${STRICT_FLAGS} \
	-DIF_REMOTE \
	-DIF_AB \
	${GL_CPPFLAGS} \
	$(IF_X_CFLAGS) \
	$(IF_TK_CFLAGS) \
	$(IF_OGL_CFLAGS) \
	$(IF_WGL_CFLAGS) \
	${TCL_CPPFLAGS}

libfb_la_LIBADD = \
	${FB_LIBS} \
	${IF_X_LIBADD} \
	${IF_OGL_LIBADD} \
	${IF_TK_LIBADD}

noinst_HEADERS = \
	dmdfb.h \
	nilcursor.h \
	oglcursor.h

dist_man_MANS = libfb.3

EXTRA_DIST = \
	if_TEMPLATE.c \
	CMakeLists.txt

DEPENDS = src/libpkg

include $(top_srcdir)/misc/Makefile.defs
