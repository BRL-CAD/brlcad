
AM_CPPFLAGS = ${TCL_CPPFLAGS}

if WITH_OPENGL
OGL_SRCS = dm-ogl.c
OGL_CFLAGS = -DDM_OGL -DIF_OGL $(GL_CPPFLAGS)
else
OGL_SRCS =
OGL_CFLAGS =
endif

if BUILD_RTGL
RTGL_SRCS = dm-rtgl.c
RTGL_CFLAGS = -DDM_RTGL -DIF_RTGL $(GL_CPPFLAGS)
else
RTGL_SRCS =
RTGL_CFLAGS =
endif

if WITH_WGL
WGL_SRCS = dm-wgl.c
WGL_CFLAGS = -DDM_WGL -DIF_WGL
else
WGL_SRCS =
WGL_CFLAGS =
endif

if WITH_X11
X11_SRCS = dm-X.c
X11_CFLAGS = -DDM_X -DIF_X
else
X11_SRCS =
X11_CFLAGS =
endif

if WITH_TK
TK_SRCS = dm-tk.c
TK_CFLAGS = -DDM_TK -DIF_TK
else
TK_SRCS =
TK_CFLAGS =
endif

points_dir = points
if WITH_PARSERS
POINTS = ${points_dir}
POINTS_CFLAGS = -DENABLE_POINTS
POINTS_LDADD = ${LIBL} $(top_builddir)/src/mged/points/libpoints.la
else
POINTS_DIR =
POINTS_CFLAGS =
POINTS_LDADD =
endif

SUBDIRS = $(POINTS)

DIST_SUBDIRS = ${points_dir}

bin_PROGRAMS = \
	cad_boundp \
	cad_parea \
	mged

mged_SOURCES = \
	adc.c \
	animedit.c \
	arbs.c \
	attach.c \
	axes.c \
	bodyio.c \
	buttons.c \
	chgmodel.c \
	chgtree.c \
	chgview.c \
	cmd.c \
	clone.c \
	color_scheme.c \
	columns.c \
	dir.c \
	$(OGL_SRCS) \
	$(RTGL_SRCS) \
	$(WGL_SRCS) \
	$(X11_SRCS) \
	$(TK_SRCS) \
	dm-generic.c \
	dm-plot.c \
	dm-ps.c \
	dodraw.c \
	doevent.c \
	dozoom.c \
	edarb.c \
	edars.c \
	edpipe.c \
	edsol.c \
	facedef.c \
	fbserv.c \
	grid.c \
	hideline.c \
	history.c \
	mater.c \
	menu.c \
	mged.c \
	mover.c \
	muves.c \
	overlay.c \
	plot.c \
	polyif.c \
	predictor.c \
	rect.c \
	rtif.c \
	scroll.c \
	set.c \
	setup.c \
	share.c \
	solids_on_ray.c \
	tedit.c \
	titles.c \
	track.c \
	update.c \
	usepen.c \
	utility1.c \
	vparse.c \
	vrlink.c

# per-program CPPFLAGS wasn't added until automake 1.7
mged_CFLAGS = \
	${STD_C99_FLAGS} \
	${STRICT_FLAGS} \
	$(OGL_CFLAGS) \
	$(RTGL_CFLAGS) \
	$(WGL_CFLAGS) \
	$(X11_CFLAGS) \
	$(TK_CFLAGS) \
	$(POINTS_CFLAGS) \
	${PNG_CPPFLAGS}	\
	${ITCL_CPPFLAGS} \
	${ITK_CPPFLAGS} \
	${TK_CPPFLAGS} \
	-I$(top_srcdir)/src/other/tcl/generic \
	-I$(top_srcdir)/src/other/tcl/unix \
	-I$(top_srcdir)/src/other/tk/generic \
	-I$(top_srcdir)/src/other/tk/unix

if WITH_TK
mged_tk_libs = \
	${ITK} \
	${TK}
else
mged_tk_libs =
endif

mged_LDADD = \
	${POINTS_LDADD} \
	${TERMIO} \
	${WDB} \
	${GED} \
	${DM} ${DM_LIBS} \
	${ITCL} \
	${TCLCAD} \
	${mged_tk_libs}

EXTRA_PROGRAMS = sged
sged_SOURCES = $(mged_SOURCES)
sged_LDFLAGS = -static
sged_CFLAGS = $(mged_CFLAGS)
sged_LDADD = $(mged_LDADD)

cad_boundp_SOURCES = cad_boundp.c
cad_boundp_LDADD = ${BU}

cad_parea_SOURCES = cad_parea.c
cad_parea_LDADD = ${BU}

noinst_HEADERS = \
	cmd.h \
	comb_bool.h \
	fbserv.h \
	htmlLibraryUi.h \
	joints.h \
	menu.h \
	mgdev.h \
	mged.h \
	mged_dm.h \
	qray.h \
	scroll.h \
	sedit.h \
	titles.h

man_MANS = \
	cad_boundp.1 \
	cad_parea.1

INCLUDES = \
	${X_CFLAGS} \
	-I$(top_srcdir)/include

EXTRA_DIST = \
	$(man_MANS) \
	bool_rewrite.c \
	comb_bool_parse.y \
	comb_bool_scan.l \
	dm-ogl.c \
	dm-rtgl.c \
	dm-wgl.c \
	dm-X.c \
	dm-tk.c \
	mged.bat \
	red.c \
	CMakeLists.txt

DEPENDS = src/libtermio src/libwdb src/libdm src/other/incrTcl src/libtclcad

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(cad_boundp_OBJECTS) \
	$(cad_parea_OBJECTS) \
	$(mged_OBJECTS) \
	$(bin_PROGRAMS)
