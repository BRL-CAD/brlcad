# $Id$

lib_LTLIBRARIES = libmultispectral.la

SRCS = \
	init.c \
	sh_camo.c \
	sh_light.c \
	sh_noise.c \
	sh_plastic.c \
	sh_stack.c \
	sh_temp.c

AM_CFLAGS = -DRT_MULTISPECTRAL

# Specially compiled versions
sh_camo.c: $(top_srcdir)/src/liboptical/sh_camo.c
	$(CP) $(top_srcdir)/src/liboptical/sh_camo.c .

sh_light.c: $(top_srcdir)/src/liboptical/sh_light.c
	$(CP) $(top_srcdir)/src/liboptical/sh_light.c .

sh_noise.c: $(top_srcdir)/src/liboptical/sh_noise.c
	$(CP) $(top_srcdir)/src/liboptical/sh_noise.c .

sh_plastic.c: $(top_srcdir)/src/liboptical/sh_plastic.c
	$(CP) $(top_srcdir)/src/liboptical/sh_plastic.c .

sh_stack.c: $(top_srcdir)/src/liboptical/sh_stack.c
	$(CP) $(top_srcdir)/src/liboptical/sh_stack.c .

vers.c: $(SRCS)
	sh $(top_srcdir)/sh/vers.sh libmultispectral_version "The BRL-CAD MultiSpectral Library" > $@

libmultispectral_la_SOURCES = \
	$(SRCS) \
	vers.c

libmultispectral_la_LDFLAGS = -version-info 19:1

libmultispectral_la_LIBADD = ${BU} ${TCL} ${RT}

BUILT_SOURCES = \
	sh_camo.c \
	sh_light.c \
	sh_noise.c \
	sh_plastic.c \
	sh_stack.c \
	vers.c

DISTCLEANFILES = $(BUILT_SOURCES)

