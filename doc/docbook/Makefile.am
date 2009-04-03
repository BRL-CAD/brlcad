
docbookdirs = \
	articles \
	books \
	lessons \
	system 

if BUILD_DOCS

SUBDIRS = \
	$(docbookdirs)

endif 

DIST_SUBDIRS = \
	$(docbookdirs)

EXTRA_DIST = \
	README \
	BRL-CAD_db_format.xml \
	catalog.xml \
	resources 

include $(top_srcdir)/misc/Makefile.defs
