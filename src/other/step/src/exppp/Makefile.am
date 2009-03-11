
lib_LTLIBRARIES = \
	libexppp.la
noinst_LTLIBRARIES = \
	libexppp_nil.la

bin_PROGRAMS = \
	exppp

COMMON = \
	exppp.c

exppp_SOURCES = \
	$(COMMON) \
	exppp-main.c

exppp_LDADD = ${EXPRESS} $(top_builddir)/src/express/fedex.o

libexppp_nil_la_SOURCES = $(COMMON)
libexppp_nil_la_CFLAGS = 
libexppp_nil_la_CXXFLAGS = -Wall
libexppp_nil_la_LIBADD = 

libexppp_la_SOURCES =
libexppp_la_LIBADD = libexppp_nil.la

noinst_HEADERS = 
	exppp.h

EXTRA_DIST = \
	README

DEPS=src/express
 
include $(top_srcdir)/misc/Makefile.defs

