
lib_LTLIBRARIES = \
	libexppp.la
noinst_LTLIBRARIES = \
	libexppp_nil.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/include/express \
	-I$(top_srcdir)/include/exppp \
	${NOWARN}

bin_PROGRAMS = exppp

exppp_SOURCES = exppp-main.c
exppp_LDADD = \
	${EXPRESS} \
	libexppp.la \
	$(top_builddir)/src/express/fedex.o

libexppp_nil_la_SOURCES = exppp.c
libexppp_nil_la_LDFLAGS = -avoid-version

libexppp_la_SOURCES =
libexppp_la_LDFLAGS = -no-undefined -version-info 19:1
libexppp_la_LIBADD = \
	${EXPRESS} \
	libexppp_nil.la

EXTRA_DIST = \
	README \
	CMakeLists.txt

DEPS = src/express

include $(top_srcdir)/misc/Makefile.defs

