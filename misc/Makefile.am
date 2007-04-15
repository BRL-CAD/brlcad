# $Id$

SUBDIRS = \
	archlinux \
	enigma \
	macosx \
	nsis \
	pkgconfig \
	vfont \
	win32-msvc \
	win32-msvc7

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)
sample_applications_DATA = fortran_example.f

bin_SCRIPTS = brlcad-config
man_MANS = brlcad-config.1

EXTRA_DIST = \
	$(man_MANS) \
	$(sample_applications_DATA) \
	Cakefile.defs \
	Description.txt \
	Doxyfile \
	Makefile.defs \
	batch-indent-region.el \
	brlcad-config \
	brlcad-config.in \
	brlcad.spec \
	brlcad.spec.in \
	libtool.m4 \
	rcs2log

DISTCLEANFILES = \
	brlcad-config \
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
