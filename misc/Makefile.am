# $Id$

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)
sample_applications_DATA = fortran_example.f

EXTRA_DIST = \
	$(sample_applications_DATA) \
	Cakefile.defs \
	Description.txt \
	Doxyfile \
	Makefile.defs \
	batch-indent-region.el \
	brlcad.spec.in \
	enigma \
	libtool.m4

if BUILD_PROE
PROE_DIR = pro-engineer
else !BUILD_PROE
PROE_DIR =
endif

SUBDIRS = \
	$(PROE_DIR) \
	macosx \
	vfont \
	win32-msvc \
	win32-msvc7

DISTCLEANFILES = \
	COPYING.backup \
	brlcad.spec \
	compile \
	config.guess \
	config.guess.backup \
	config.sub \
	config.sub.backup \
	install-sh \
	ltmain.sh \
	ltmain.sh.backup \
	mkinstalldirs

dist-hook:
	rm -rf `find $(distdir)/enigma -name CVS`

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

