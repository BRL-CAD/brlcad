# $Id$

SUBDIRS = \
	html \
	legal

documentationdir = $(BRLCAD_DATA)/doc

documentation_DATA = \
	IDEAS \
	PROJECTS \
	README.AIX \
	README.BSD \
	README.IRIX \
	README.Linux \
	README.MacOSX \
	README.Solaris \
	README.VAX \
	README.Windows \
	anim.txt \
	archer_ack.txt \
	benchmark.tr \
	cvs.txt \
	description.txt \
	ged.tr \
	history.txt \
	hypot.txt \
	irprep.tr \
	mater.txt \
	matrix.txt \
	mged.tr \
	mk.tr \
	movie.txt \
	old-mged.tr \
	pad_file.xml \
	pkg.tr \
	regions.txt \
	rounding.txt \
	sun.txt \
	tube.txt

man_MANS = \
	bw.5 \
	cmap.5 \
	pix.5 \
	plot3.5

EXTRA_DIST = \
	$(documentation_DATA) \
	$(man_MANS) \
	pad_file.xml.in

DISTCLEANFILES = \
	pad_file.xml

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
