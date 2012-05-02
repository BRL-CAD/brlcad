
bin_PROGRAMS = vdeck

AM_CPPFLAGS = ${TCL_CPPFLAGS} ${STRICT_FLAGS}

vdeck_LDADD = ${RT}
vdeck_SOURCES = \
	cgarbs.c \
	match.c \
	parsarg.c \
	vdeck.c

noinst_HEADERS = std.h vextern.h

man_MANS = vdeck.1

DEPENDS = src/librt

EXTRA_DIST = $(man_MANS) CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs
