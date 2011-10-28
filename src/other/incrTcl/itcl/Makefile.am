
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
	CMakeLists.txt \
	CMake/CheckCFileRuns.cmake \
	CMake/CheckPrototypeExists.cmake \
	CMake/CheckSystemFunctionality.cmake \
	CMake/ResolveCompilerPaths.cmake \
	CMake/ac_std_funcs.cmake \
	CMake/tcl.cmake

DISTCLEANFILES = pkgIndex.tcl

include $(top_srcdir)/misc/Makefile.defs

