# $Id$

adrtd = libcommon \
	librender \
	libtexture \
	libutil \
	bench \
	rise \
	isst

always_compile = \
	libtie \
	libtienet

# always compile libite
SUBDIRS = ${always_compile}
if BUILD_ADRT
SUBDIRS += ${adrtd}
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
