# $Id$
lib_LTLIBRARIES = libdm.la

SRCS = clip.c color.c dm-Null.c dm-X.c dm-generic.c dm-plot.c dm-ps.c dm_obj.c knob.c options.c query.c tcl.c 
libdm_la_SOURCES = $(SRCS) vers.c

CFLAGS = @X_CFLAGS@ -I$(top_srcdir)/h

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	$(top_srcdir)/sh/newvers.sh dm_version libdm
