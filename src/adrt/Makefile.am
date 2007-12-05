# $Id$

adrtd = libcommon \
	librender \
	libtexture \
	bench \
	rise \
	isst

always_compile = \
	libtie \
	libtienet \
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
