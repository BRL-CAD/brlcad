# $Id$

tklibdir = ${libdir}

tklib_DATA = tkConfig.sh

noinst_LTLIBRARIES = libtku.la libwish.la

libtku_la_LDFLAGS = -static
libtku_la_SOURCES = \
	tkUnix.c \
	tkUnix3d.c \
	tkUnixButton.c \
	tkUnixColor.c \
	tkUnixConfig.c \
	tkUnixCursor.c \
	tkUnixDialog.c \
	tkUnixDraw.c \
	tkUnixEmbed.c \
	tkUnixEvent.c \
	tkUnixFocus.c \
	tkUnixFont.c \
	tkUnixInit.c \
	tkUnixKey.c \
	tkUnixMenu.c \
	tkUnixMenubu.c \
	tkUnixScale.c \
	tkUnixScrlbr.c \
	tkUnixSelect.c \
	tkUnixSend.c \
	tkUnixWm.c \
	tkUnixXId.c

libwish_la_LDFLAGS = -static
libwish_la_SOURCES = tkAppInit.c

EXTRA_DIST = \
	README \
	aclocal.m4 \
	license.terms \
	mkLinks \
	tcl.m4 \
	tk.spec \
	tkConfig.sh.in

noinst_HEADERS = \
	tkUnixDefault.h \
	tkUnixInt.h \
	tkUnixPort.h

AM_CPPFLAGS = \
	-DTK_LIBRARY="\"${TK_LIBRARY}\"" \
	-I${top_srcdir}/src/other/libtcl/generic \
	-I${top_srcdir}/src/other/libtk/bitmaps \
	-I${top_srcdir}/src/other/libtk/generic \
	-I${top_srcdir}/src/other/libtk/unix \
	-I${top_srcdir}/src/other/libtk/mac \
	-I${top_srcdir}/src/other/libtk/win \
	@X_CFLAGS@

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

