# $Id$

adrtd = libtienet \
	libcommon \
	librender \
	libtexture \
	libutil \
	bench \
	rise \
	isst

# always compile libite
if BUILD_ADRT
SUBDIRS = \
	libtie \
	${adrtd}
else !BUILD_ADRT
SUBDIRS = \
	libtie
endif
# end !BUILD_ADRT

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
	config.h \
	configure.ac \
	COPYING \
	Doxyfile \
	NEWS \
	README

DEPS = src/libbu

include $(top_srcdir)/misc/Makefile.defs
