
bin_PROGRAMS = vdeck

AM_CPPFLAGS = \
	${STD_C99_FLAGS} \
	${TCL_CPPFLAGS} \
	${STRICT_FLAGS}

vdeck_LDADD = ${RT}
vdeck_SOURCES = \
	cgarbs.c \
	match.c \
	parsarg.c \
	vdeck.c

noinst_HEADERS = std.h vextern.h

DEPENDS = src/librt

EXTRA_DIST = $(man_MANS) CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs
