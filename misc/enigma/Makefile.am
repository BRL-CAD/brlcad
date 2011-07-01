
bin_PROGRAMS = enigma

enigma_SOURCE = enigma.c
enigma_LDFLAGS = ${LIBBSD}

dist_man_MANS = enigma.1

EXTRA_DIST = \
	AUTHORS \
	COPYING \
	ChangeLog \
	INSTALL \
	NEWS \
	README \
	crypt.c \
	configure.ac \
	CMakeLists.txt

DISTCLEANFILES = \
	aclocal.m4 \
	config.log \
	config.status \
	configure

noprod: prodclean

prodclean-recursive:

prodclean: prodclean-recursive
	rm -f enigma

fast: all-am

depends: all-am


# Local Variables:
# mode: Makefile
# tab-width: 8
# indent-tabs-mode: t
# End:
# ex: shiftwidth=8 tabstop=8
