
if BUILD_DOCS
DOCBOOK_DIR = docbook
else
DOCBOOK_DIR =
endif

SUBDIRS = \
	${DOCBOOK_DIR} \
	book \
	html \
	legal

DIST_SUBDIRS = \
	book \
	docbook \
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
	TODO.BREP \
	anim.txt \
	archer_ack.txt \
	benchmark.tr \
	brep.txt \
        BRL-CAD.bib\
	cvs.txt \
	description.txt \
	deprecation.txt \
	ged.tr \
	history.txt \
	hypot.txt \
	irprep.tr \
	mater.txt \
	matrix.txt \
	mged.sh \
	mk.tr \
	movie.txt \
	old-mged.tr \
	pad_file.xml \
	pkg.tr \
	regions.txt \
	rounding.txt \
	sun.txt \
	tube.txt \
	tool_categories.txt

man_MANS = \
	bw.5 \
	cmap.5 \
	pix.5 \
	plot3.5

EXTRA_DIST = \
	$(documentation_DATA) \
	$(man_MANS) \
	STARTERS \
	pad_file.xml.in

DISTCLEANFILES = \
	pad_file.xml

include $(top_srcdir)/misc/Makefile.defs
