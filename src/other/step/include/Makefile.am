
SUBDIRS = \
	express \
	exppp

# As yet the status of most of scl's headers is
# not clarified - the C++ headers are shared among
# the various cl* subdirectory builds so for the
# moment they are left there.  At some point the
# public headers should be identified and placed
# in subdirectories here.

noinst_HEADERS = \
	sclprefixes.h 


include $(top_srcdir)/misc/Makefile.defs

EXTRA_DIST = CMakeLists.txt scl_cf_cmake.h.in
