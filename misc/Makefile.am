# $Id$

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)
sample_applications_DATA = fortran_example.f

SUBDIRS = \
	archlinux \
	macosx \
	vfont \
	win32-msvc \
	win32-msvc7

EXTRA_DIST = \
	$(sample_applications_DATA) \
	Cakefile.defs \
	Description.txt \
	Doxyfile \
	Makefile.defs \
	batch-indent-region.el \
	brlcad.spec \
	brlcad.spec.in \
	enigma \
	libtool.m4 \
	rcs2log

DISTCLEANFILES = \
	brlcad.spec \
	compile \
	config.guess \
	config.guess.backup \
	config.sub \
	config.sub.backup \
	install-sh \
	ltmain.sh \
	ltmain.sh.backup \
	mkinstalldirs \
	py-compile

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
