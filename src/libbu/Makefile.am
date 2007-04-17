# $Id$

lib_LTLIBRARIES = libbu.la
noinst_LTLIBRARIES = libbu_nil.la

noinst_PROGRAMS = htester

libbu_nil_la_SOURCES = \
	association.c \
	avs.c \
	badmagic.c \
	basename.c \
	bitv.c \
	bomb.c \
	brlcad_path.c \
	bu_tcl.c \
	cmd.c \
	cmdhist.c \
	cmdhist_obj.c \
	color.c \
	convert.c \
	dirname.c \
	fgets.c \
	fopen_uniq.c \
	getopt.c \
	hash.c \
	hist.c \
	hook.c \
	htond.c \
	htonf.c \
	image.c \
	ispar.c \
	lex.c \
	linebuf.c \
	list.c \
	log.c \
	magic.c \
	malloc.c \
	mappedfile.c \
	mread.c \
	mro.c \
	observer.c \
	parallel.c \
	parse.c \
	printb.c \
	ptbl.c \
	rb_create.c \
	rb_delete.c \
	rb_diag.c \
	rb_extreme.c \
	rb_free.c \
	rb_insert.c \
	rb_order_stats.c \
	rb_rotate.c \
	rb_search.c \
	rb_walk.c \
	semaphore.c \
	stat.c \
	units.c \
	vers.c \
	vfont.c \
	vls.c \
	whereis.c \
	which.c \
	xdr.c

libbu_la_SOURCES =
libbu_la_LDFLAGS = -version-info 19:1
libbu_la_LIBADD = \
	libbu_nil.la

htester_SOURCES = htester.c
htester_LDADD = \
	libbu.la \
	${BU} ${BU_LIBS}

include_HEADERS = rb_internals.h

man_MANS = \
	htond.3 \
	libbu.3 \
	redblack.3

EXTRA_DIST = \
	$(man_MANS) \
	libbu.dsp

AM_CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	${PNG_CPPFLAGS} \
	${TCL_CPPFLAGS}

DEPS=include
if BUILD_TCL
DEPS+=src/other/tcl
endif

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libbu_nil_la_OBJECTS) \
	$(libbu_la_OBJECTS) \
	$(htester_OBJECTS) \
	$(noinst_PROGRAMS)
