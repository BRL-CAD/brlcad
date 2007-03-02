# $Id$

lib_LTLIBRARIES = libbu.la
noinst_LTLIBRARIES = libbu_nil.la

noinst_PROGRAMS = htester

SRCS = \
	association.c \
	avs.c \
	badmagic.c \
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
	vfont.c \
	vls.c \
	whereis.c \
	which.c \
	xdr.c

libbu_nil_la_SOURCES = \
	$(SRCS) \
	vers.c

libbu_la_SOURCES =
libbu_la_LDFLAGS = -version-info 19:1
libbu_la_LIBADD = \
	libbu_nil.la \
	${LIBMALLOC} \
	${LIBTHREAD}

htester_SOURCES = htester.c
htester_LDADD = libbu.la

include_HEADERS = rb_internals.h

man_MANS = \
	htond.3 \
	libbu.3 \
	redblack.3

EXTRA_DIST = \
	$(man_MANS) \
	libbu.dsp \
	vers_win.c

AM_CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	${TCL_CPPFLAGS}

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh bu_version "The BRL-CAD Utility Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

if BUILD_TCL
DEPS=src/other/tcl
else
DEPS=
endif

include $(top_srcdir)/misc/Makefile.defs
