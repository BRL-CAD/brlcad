
lib_LTLIBRARIES = libtclcad.la

if WITH_WGL
DM_WGL_CFLAGS = -DDM_WGL -DIF_WGL
else
DM_WGL_CFLAGS =
endif

if WITH_X11
DM_X_CFLAGS = -DDM_X -DIF_X \
	${X_CFLAGS}
else
DM_X_CFLAGS =
endif

if WITH_OPENGL
DM_OGL_CFLAGS = -DDM_OGL -DIF_OGL $(GL_CPPFLAGS)
DM_OGL_LIBS = ${LIBGL}
else
DM_OGL_CFLAGS =
DM_OGL_LIBS =
endif

if WITH_TK
DM_TK_CFLAGS = -DDM_TK -DIF_TK
else
DM_TK_CFLAGS =
endif

libtclcad_la_SOURCES = \
	tclcad.c \
	tclcadAutoPath.c \
	tkImgFmtPIX.c \
	tclcad_obj.c

libtclcad_la_CFLAGS = \
	${STD_C99_FLAGS} \
	${STRICT_FLAGS} \
	${DM_X_CFLAGS} \
	${DM_TK_CFLAGS} \
	${DM_WGL_CFLAGS} \
	${DM_OGL_CFLAGS} \
	${PNG_CPPFLAGS} \
	${X_CFLAGS} \
	${ITK_CPPFLAGS} \
	${ITCL_CPPFLAGS} \
	${TK_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	-I$(top_srcdir)/src/other/tcl/generic \
	-I$(top_srcdir)/src/other/tcl/unix \
	-I$(top_srcdir)/src/other/tk/generic \
	-I$(top_srcdir)/src/other/tk/unix
libtclcad_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
libtclcad_la_LIBADD = ${TCLCAD_LIBS}

EXTRA_DIST = \
	CMakeLists.txt

DEPENDS = src/libdm src/libged src/other/incrTcl

include $(top_srcdir)/misc/Makefile.defs
