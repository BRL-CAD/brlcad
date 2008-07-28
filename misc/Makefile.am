# $Id: Makefile.am 30974 2008-05-04 04:37:42Z brlcad $

SUBDIRS = \
	archlinux \
	debian \
	enigma \
	macosx \
	nsis \
	pkgconfig \
	win32-msvc \
	win32-msvc8

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
	doxygen_structure \
	flawfinder \
	libtool.m4

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
	py-compile \
	ylwrap

include $(top_srcdir)/misc/Makefile.defs
