# $Id$

SUBDIRS = \
	libtie \
	libtienet \
	libcommon \
	librender \
	libtexture \
	libutil \
	bench \
	rise \
	isst

DIST_SUBDIRS = \
	$(SUBDIRS) \
	doc \
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

MAINTAINERCLEANFILES = Makefile.in

DEPS=src/libbu

include $(top_srcdir)/misc/Makefile.defs
