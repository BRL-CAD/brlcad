# $Id$

bin_PROGRAMS = lgt

AM_CPPFLAGS = ${TCL_CPPFLAGS}

lgt_SOURCES = \
	char.c \
	do_options.c \
	error.c \
	execshell.c \
	fb.c \
	getfont.c \
	glob.c \
	grid_rotate.c \
	hmenu.c \
	ir.c \
	lgt.c \
	lgt_db.c \
	mat_db.c \
	octree.c \
	prnt.c \
	reflect.c \
	sgi_dep.c \
	texture.c \
	trie.c

noinst_HEADERS = \
	ascii.h \
	cursors.h \
	extern.h \
	font.h \
	hmenu.h \
	lgt.h \
	mat_db.h \
	screen.h \
	texture.h \
	tree.h \
	vecmath.h

lgt_LDADD = \
	${RT} ${RT_LIBS} \
	${FB} ${FB_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${PKG} ${PKG_LIBS} \
	${TERMIO} ${TERMIO_LIBS} \
	${CURSOR} ${CURSOR_LIBS} \
	${LIBSGIGL} \
	${SYSV} ${SYSV_LIBS}

lgt_CFLAGS = -DBEHIND_ME_TOL=0.05

dist_man_MANS = lgt.1

EXTRA_DIST = resource.c

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(lgt_OBJECTS) \
	$(bin_PROGRAMS)
