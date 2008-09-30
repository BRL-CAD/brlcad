
docbookdirs = \
	articles \
	books \
	lessons \
	system 

if BUILD_DOCS

SUBDIRS = $(docbookdirs)

else !BUILD_DOCS

DIST_SUBDIRS = $(docbookdirs)

endif
# end BUILD_DOCS

include $(top_srcdir)/misc/Makefile.defs
