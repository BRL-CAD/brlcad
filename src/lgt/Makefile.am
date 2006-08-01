# $Id$

bin_PROGRAMS = lgt

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
	${RT} \
	${FB} \
	${TERMIO} \
	${CURSOR} \
	${LIBSGIGL} \
	${SYSV}

lgt_CFLAGS = -DBEHIND_ME_TOL=0.05

dist_man_MANS = lgt.1

EXTRA_DIST = resource.c

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(lgt_OBJECTS) \
	$(bin_PROGRAMS)
