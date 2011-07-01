
bin_PROGRAMS = fbed

fbed_SOURCES = \
	char.c \
	empty.c \
	execshell.c \
	fbed.c \
	fill_buf.c \
	getinput.c \
	glob.c \
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
	popup.h \
	std.h \
	try.h

AM_CPPFLAGS = \
	${TERMLIB_CPPFLAGS} \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

fbed_LDADD = \
	$(top_builddir)/src/vfont/libvfont_nil.la \
	${FB} \
	${TERMIO} \
	${CURSOR} \
	${TERMLIB} \
	${LIBSGIGL}

man_MANS = fbed.1

EXTRA_DIST = $(man_MANS) CMakeLists.txt

DEPENDS = src/vfont

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(fbed_OBJECTS) \
	$(bin_PROGRAMS)
