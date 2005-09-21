
noinst_LTLIBRARIES = pngtcl.la

pngtcl_la_LDFLAGS = -module
pngtcl_la_SOURCES = \
	pngtcl.c \
	pngtclStubInit.c \
	pngtclStubLib.c

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/tkimg/pngtcl \
	-I${top_srcdir}/src/other/tkimg/zlibtcl \
	-DPNG_PACKAGE_NAME=\"libpng\" \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
