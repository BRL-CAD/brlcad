# $Id$
lib_LTLIBRARIES = libfb.la

SRCS = fb_generic.c fb_log.c fb_obj.c fb_paged_io.c fb_rect.c fbserv_obj.c fb_util.c getput.c if_ab.c if_debug.c if_disk.c if_mem.c if_null.c if_remote.c if_stack.c if_X24.c if_X.c server.c tcl.c if_ogl.c

libfb_la_SOURCES = $(SRCS) vers.c
libfb_la_LDFLAGS = -version-info 19:1

include_HEADERS = adage.h adagecursor.h adageframe.h dmdfb.h fblocal.h nilcursor.h oglcursor.h pkgtypes.h sgicursor.h

AM_CFLAGS = @X_CFLAGS@ -I$(top_srcdir)/h -DIF_REMOTE -DIF_AB -DIF_X
libfb_la_LIBADD = @PKG@ @X_LIBS@

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	$(top_srcdir)/sh/newvers.sh fb_version libfb

dist_man_MANS = libfb.3
