# $Id$

lib_LTLIBRARIES = libmultispectral.la

SRCS = \
	init.c \
	sh_temp.c

libmultispectral_la_SOURCES = \
	$(SRCS) \
	vers.c

libmultispectral_la_LDFLAGS = -version-info 19:1

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	sh $(top_srcdir)/sh/vers.sh libmultispectral_version "The BRL-CAD MultiSpectral Library" > $@
