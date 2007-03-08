# $Id$

bin_PROGRAMS = fbed

fbed_SOURCES = \
	char.c \
	empty.c \
	execshell.c \
	fbed.c \
	fill_buf.c \
	getfont.c \
	getinput.c \
	glob.c \
	loadfont.c \
	pos_pad.c \
	prnt.c \
	sgi_dep.c \
	squash.c \
	try.c

noinst_HEADERS = \
	ascii.h \
	cursorbits.h \
	extern.h \
	fb_ik.h \
	font.h \
	popup.h \
	std.h \
	try.h

fbed_CFLAGS = ${TERMLIB_CPPFLAGS}
fbed_LDADD = \
	${FB} \
	${BU} \
	${PKG} \
	${TERMIO} \
	${CURSOR} \
	${TERMLIB} \
	${LIBSGIGL}

man_MANS = fbed.1

EXTRA_DIST = \
	$(man_MANS) \
	font.doc

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(fbed_OBJECTS) \
	$(bin_PROGRAMS)
