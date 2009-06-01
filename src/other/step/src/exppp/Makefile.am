
lib_LTLIBRARIES = \
	libexppp.la
noinst_LTLIBRARIES = \
	libexppp_nil.la

AM_CFLAGS = -I$(top_srcdir)/include/express -I$(top_srcdir)/include/exppp 

bin_PROGRAMS = \
	exppp

COMMON = \
	exppp.c

exppp_SOURCES = \
	exppp-main.c

exppp_LDADD = \
	${EXPRESS} \
	libexppp.la \
	$(top_builddir)/src/express/fedex.o

libexppp_nil_la_SOURCES = $(COMMON)
libexppp_nil_la_LDFLAGS = -avoid-version

libexppp_la_SOURCES =
libexppp_la_LDFLAGS = -avoid-version -no-undefined
libexppp_la_LIBADD = \
	${EXPRESS} \
	libexppp_nil.la

EXTRA_DIST = \
	README

DEPS=src/express
 
include $(top_srcdir)/misc/Makefile.defs

