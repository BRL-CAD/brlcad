# $Id$

bin_PROGRAMS = halftone

halftone_SOURCES = \
	main.c \
	sharpen.c \
	tone_classic.c \
	tone_floyd.c \
	tone_folly.c \
	tone_simple.c \
	tonescale.c

halftone_LDADD = ${LIBRT} ${FB}

man_MANS = halftone.1

EXTRA_DIST = $(man_MANS)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(halftone_OBJECTS) \
	$(bin_PROGRAMS)
