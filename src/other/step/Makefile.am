AUTOMAKE_OPTIONS = 1.6 dist-zip dist-bzip2
ACLOCAL_AMFLAGS = -I m4

scl_dirs = \
	src \
	include \
	m4 \
	misc \
	doc \
	data 

SUBDIRS = \
	$(scl_dirs) 

DIST_SUBDIRS = \
	$(scl_dirs) 

documentationdir = $(SCL_DATA)

documentation_DATA = \
	AUTHORS \
	COPYING \
	INSTALL \
	NEWS \
	README

EXTRA_DIST = \
	$(documentation_DATA) \
	config \
	ChangeLog \
	libtool \
	configure.ac \
	TODO \
	CMakeLists.txt  \
	CMake/CheckCFileRuns.cmake \
	CMake/FindLEX.cmake \
	CMake/FindYACC.cmake \
	CMake/SCL_Utils.cmake \
	CMake/test_srcs/sys_wait_test.c

DISTCLEANFILES = \
	$(CONFIG_CACHE) \
	aclocal.m4 \
	aclocal.m4~ \
	config.log \
	config.status \
	configure \
	install.$(host_triplet).log \
	libtool \
	so_locations

include $(top_srcdir)/misc/Makefile.defs

# Local Variables:
# mode: Makefile
# tab-width: 8
# indent-tabs-mode: t
# End:
# ex: shiftwidth=8 tabstop=8
