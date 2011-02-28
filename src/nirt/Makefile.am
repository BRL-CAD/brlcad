
SUBDIRS = \
        sfiles

bin_PROGRAMS = \
	nirt \
	showshot

AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

nirt_SOURCES = \
	nirt.c \
	command.c \
	conversion.c \
	if.c \
	interact.c \
	bsphere.c \
	read_mat.c \
	str_manip.c \
	parse_fmt.c

nirt_CFLAGS = -DDFLT_SHELL=\"/bin/sh\"
nirt_LDADD = ${FB} ${RT}

showshot_SOURCES = showshot.c
showshot_LDADD = ${BN}

man_MANS = \
	showshot.1

noinst_HEADERS = \
	nirt.h \
	usrfmt.h

EXTRA_DIST = \
	$(man_MANS) \
	CMakeLists.txt \
	dist_def.c

DEPENDS = src/libfb src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(nirt_OBJECTS) \
	$(showshot_OBJECTS) \
	$(bin_PROGRAMS)
