# $Id$

noinst_LTLIBRARIES = libtkunix.la

libtkunix_la_SOURCES = \
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

tklib_DATA = tkConfig.sh

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/libtk/generic \
	-DTK_LIBRARY="\"${TK_LIBRARY}\""

noinst_HEADERS = \
	tkUnixDefault.h \
	tkUnixInt.h \
	tkUnixPort.h

EXTRA_DIST = \
	README \
	mkLinks \
	tkAppInit.c \
	tkConfig.sh.in

MAINTAINERCLEANFILES = \
	Makefile.in

brlcadincludedir = $(includedir)/brlcad

tklibdir = ${libdir}
tkdir = ${prefix}/tk8.4

