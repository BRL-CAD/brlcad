# $Id$

adrtd = libcommon \
	librender \
	bench \
	rise \
	isst

always_compile = \
	libtie \
	libtienet \
	libtexture \
	libutil

if BUILD_ADRT
SUBDIRS = ${adrtd} ${always_compile}
else
SUBDIRS = ${always_compile}
endif

DIST_SUBDIRS = \
	${adrtd} \
	doc \
	libtie \
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
	acinclude.m4 \
	AUTHORS \
	ChangeLog \
	configure.ac \
	COPYING \
	Doxyfile \
	NEWS \
	README

DEPS = src/libbu

include $(top_srcdir)/misc/Makefile.defs
