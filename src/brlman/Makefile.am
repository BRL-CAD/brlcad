
dist_bin_SCRIPTS = brlman

brlman: brlman.sh
	cp brlman.sh brlman

CLEANFILES = brlman

man_MANS = brlman.1
EXTRA_DIST = \
	$(man_MANS) \
	$(dist_bin_SCRIPTS) \
	brlman.sh.in \
	CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs
