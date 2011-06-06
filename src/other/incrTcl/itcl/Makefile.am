
SUBDIRS = \
	doc \
	generic \
	library

brlcadscriptdir = $(libdir)/itcl@ITCL_VERSION@
brlcadscript_SCRIPTS = pkgIndex.tcl

EXTRA_DIST = \
	$(brlcadscript_SCRIPTS) \
	aclocal.m4 \
	itclConfig.sh.in \
	license.terms \
	tests \
	win \
	CMakeLists.txt

DISTCLEANFILES = pkgIndex.tcl

include $(top_srcdir)/misc/Makefile.defs

