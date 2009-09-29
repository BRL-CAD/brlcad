
nonexpressdirs = \
	clutils \
	clstepcore \
	cldai \
	cleditor \
	exppp \
	fedex_plus

if ONLY_EXPRESS
SUBDIRS = express

else !ONLY_EXPRESS
SUBDIRS = \
	express \
	$(nonexpressdirs)
endif

EXTRA_DIST = \
	clSchemas \
	clivfasd \
	clprobe-ui \
	test

DIST_SUBDIRS = \
	express \
	$(nonexpressdirs)

include $(top_srcdir)/misc/Makefile.defs

# Local Variables:
# mode: Makefile
# tab-width: 8
# indent-tabs-mode: t
# End:
# ex: shiftwidth=8 tabstop=8
