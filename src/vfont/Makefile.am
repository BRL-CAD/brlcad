
noinst_LTLIBRARIES = libvfont_nil.la

libvfont_nil_la_SOURCES = getfont.c

noinst_HEADERS = vfont.h

fontdir = $(BRLCAD_DATA)/vfont
font_DATA = \
	fix.6r \
	nonie.r.12

EXTRA_DIST = $(font_DATA) CMakeLists.txt

AM_CPPFLAGS = ${TCL_CPPFLAGS}

include $(top_srcdir)/misc/Makefile.defs
