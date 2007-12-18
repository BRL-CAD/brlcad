# $Id$

adrtd = libcommon \
	bench \
	rise \
	isst

always_compile = \
	libtie \
	libtienet \
	libtexture \
	librender \
	libutil

if BUILD_ADRT
SUBDIRS = \
	${always_compile} \
	${adrtd}
else
SUBDIRS = ${always_compile}
endif

DIST_SUBDIRS = \
	${adrtd} \
	${always_compile} \
	doc \
	misc \
	scripts

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = adrt.h adrt_struct.h

if HAVE_DOXYGEN
doxygen:
	mkdir doxygen
	doxygen Doxyfile
endif

EXTRA_DIST = \
	AUTHORS \
	COPYING \
	Doxyfile \
	NEWS \
	README

DEPS = src/libbu

include $(top_srcdir)/misc/Makefile.defs
