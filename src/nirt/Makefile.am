# $Id$

bin_PROGRAMS = \
	nirt \
	showshot

nirt_srcs = \
	nirt.c \
	command.c \
	conversion.c \
	if.c \
	interact.c \
	bsphere.c \
	read_mat.c \
	str_manip.c \
	parse_fmt.c

nirt_LDADD = ${LIBRT} ${BN} ${FB}
nirt_CFLAGS = -DDFLT_SHELL=\"/bin/sh\"
nirt_SOURCES = \
	$(nirt_srcs) \
	vers.c

showshot_LDADD = ${BN}
showshot_SOURCES = showshot.c

man_MANS = \
	nirt.1 \
	showshot.1

noinst_HEADERS = \
	nirt.h \
	usrfmt.h

EXTRA_DIST = \
	$(man_MANS) \
	dist_def.c \
	nirt.dsp

BUILT_SOURCES = vers.c
vers.c: $(nirt_srcs) Makefile.am
	sh $(top_srcdir)/sh/vers.sh version "Natalie's Interactive Ray Tracer" > $@

CLEANFILES = vers.c
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(nirt_OBJECTS) \
	$(showshot_OBJECTS) \
	$(bin_PROGRAMS)
