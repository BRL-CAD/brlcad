# $Id$

bin_PROGRAMS = fbed

BU=$(top_builddir)/libbu/libbu.la @TCL_LIBS@ @PTHREAD_LIBS@ -lm
FB=$(top_builddir)/libfb/libfb.la @X_LIBS@ $(PKG) $(BU)
TERMIO=$(top_builddir)/libtermio/libtermio.la
CURSOR=$(top_builddir)/libcursor/libcursor.la
PKG=$(top_builddir)/libpkg/libpkg.la

fbed_SOURCES = char.c empty.c execshell.c fbed.c fill_buf.c getfont.c getinput.c glob.c loadfont.c pos_pad.c prnt.c sgi_dep.c squash.c try.c
noinst_HEADERS = ascii.h cursorbits.h extern.h fb_ik.h font.h popup.h std.h try.h
fbed_LDADD = ${FB} ${TERMIO} ${CURSOR} -lncurses

#include "../Cakefile.rules"
