# $Id$

SUBDIRS = libtie libtienet libcommon librender libtexture libutil rise igvt


if HAVE_DOXYGEN
doxygen:
	mkdir doxygen
	doxygen Doxyfile
endif

# redhat's automake 1.6.3 is horribly broken. it builds the
# Makefile.in BEFORE installing depcomp, and since depcomp
# isn't installed at that time, it does NOT include depcomp in
# the dist var's. The end result is that depcomp doesn't get in
# to the tarball if you don't run automake 'hard' (automake -a
# instead of automake -a --no-force).
EXTRA_DIST=depcomp
