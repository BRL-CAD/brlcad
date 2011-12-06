
lib_LTLIBRARIES = libbu.la
noinst_LTLIBRARIES = libbu_nil.la

#	test_file
noinst_PROGRAMS = \
	test_basename \
	test_dirname \
	test_htond \
	test_progname \
	test_quote \
	test_timer \
	test_vls

libbu_nil_la_SOURCES = \
	argv.c \
	avs.c \
	backtrace.c \
	badmagic.c \
	basename.c \
	bitv.c \
	bomb.c \
	booleanize.c \
	brlcad_path.c \
	cmd.c \
	cmdhist.c \
	color.c \
	convert.c \
	crashreport.c \
	dirent.c \
	dirname.c \
	dlfcn.c \
	endian.c \
	fchmod.c \
	fgets.c \
	file.c \
	fnmatch.c \
	fopen_uniq.c \
	getcwd.c \
	getopt.c \
	globals.c \
	hash.c \
	hist.c \
	hook.c \
	htond.c \
	htonf.c \
	image.c \
	interrupt.c \
	ispar.c \
	kill.c \
	lex.c \
	linebuf.c \
	list.c \
	log.c \
	magic.c \
	malloc.c \
	mappedfile.c \
	mread.c \
	observer.c \
	parallel.c \
	parse.c \
	pool.c \
	printb.c \
	process.c \
	progname.c \
	ptbl.c \
	quote.c \
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
	simd.c \
	str.c \
	tcl.c \
	temp.c \
	timer.c \
	units.c \
	vers.c \
	vfont.c \
	vlb.c \
	vls.c \
	whereis.c \
	which.c \
	xdr.c

libbu_la_SOURCES =
libbu_la_LDFLAGS = -version-info 19:1 -no-undefined
libbu_la_LIBADD = \
	libbu_nil.la \
	${BU_LIBS}

# test_file_SOURCES = test_file.c
# test_file_LDADD = \
# 	libbu.la \
# 	${BU_LIBS}

test_htond_SOURCES = test_htond.c
test_htond_LDADD = \
	libbu.la \
	${BU_LIBS}

test_timer_SOURCES = test_timer.c
test_timer_LDADD = \
	libbu.la \
	${BU_LIBS}

test_basename_SOURCES = test_basename.c
test_basename_LDADD = \
	libbu.la \
	${BU_LIBS}

test_dirname_SOURCES = test_dirname.c
test_dirname_LDADD = \
	libbu.la \
	${BU_LIBS}

test_progname_SOURCES = test_progname.c
test_progname_LDADD = \
	libbu.la \
	${BU_LIBS}

test_quote_SOURCES = test_quote.c
test_quote_LDADD = \
	libbu.la \
	${BU_LIBS}

test_vls_SOURCES = test_vls.c
test_vls_LDADD = \
	libbu.la \
	${BU_LIBS}

noinst_HEADERS = \
	rb_internals.h

man_MANS = \
	htond.3 \
	libbu.3 \
	redblack.3

EXTRA_DIST = \
	$(man_MANS) \
	uce_dirent.h \
	CMakeLists.txt

AM_CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-I${top_srcdir}/src/other/uce-dirent \
	${PNG_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	${LIBZ_CPPFLAGS}

AM_CFLAGS = ${STRICT_FLAGS}

if BUILD_TCL
TCLDEP = $(LDEP) src/other/tcl
else
TCLDEP =
endif

if BUILD_PNG
PNGDEP = $(LDEP) src/other/libpng
else
PNGDEP =
endif

DEPENDS = include $(TCLDEP) $(PNGDEP)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libbu_nil_la_OBJECTS) \
	$(libbu_la_OBJECTS) \
	$(lib_LTLIBRARIES) \
	$(noinst_LTLIBRARIES) \
	$(noinst_PROGRAMS)
