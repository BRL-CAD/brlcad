
lib_LTLIBRARIES = libdm.la

if WITH_WGL
DM_WGL_CFLAGS = -DDM_WGL -DIF_WGL
else
DM_WGL_CFLAGS =
endif

if WITH_X11
DM_X_CFLAGS = -DDM_X -DIF_X ${X_CFLAGS}
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

if BUILD_RTGL
DM_RTGL_CFLAGS = -DDM_RTGL -DIF_RTGL $(GL_CPPFLAGS)
DM_RTGL_LIBS = ${LIBGL}
else
DM_RTGL_CFLAGS = 
DM_RTGL_LIBS = 
endif

if WITH_TK
DM_TK_CFLAGS = -DDM_TK -DIF_TK
else
DM_TK_CFLAGS =
endif


libdm_la_SOURCES = \
	adc.c \
	axes.c \
	clip.c \
	color.c \
	dm-Null.c \
	dm-X.c \
	dm-tk.c \
	dm-generic.c \
	dm-ogl.c \
	dm-rtgl.c \
	dm-plot.c \
	dm-ps.c \
	dm-wgl.c \
	dm_obj.c \
	focus.c \
	grid.c \
	knob.c \
	labels.c \
	options.c \
	query.c \
	rect.c \
	scale.c \
	tcl.c \
	vers.c

libdm_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}

# per-program CPPFLAGS wasn't added until automake 1.7
libdm_la_CFLAGS = \
	${STD_C99_FLAGS} \
	${STRICT_FLAGS} \
	${DM_X_CFLAGS} \
	${DM_TK_CFLAGS} \
	${DM_WGL_CFLAGS} \
	${DM_OGL_CFLAGS} \
	${DM_RTGL_CFLAGS} \
	${PNG_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS}

libdm_la_LIBADD = \
	${DM_LIBS} \
	${DM_OGL_LIBS} \
	${DM_RTGL_LIBS}

EXTRA_DIST = \
	TODO \
	CMakeLists.txt

DEPENDS = src/librt src/libfb src/libged

include $(top_srcdir)/misc/Makefile.defs
