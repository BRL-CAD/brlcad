# $Id$

EXTRA_DIST = \
	ChangeLog \
	ChangeLog.2002 \
	ChangeLog.2004 \
	README \
	bitmaps \
	changes \
	compat \
	doc \
	generic \
	library \
	license.terms \
	macosx \
	tests \
	unix \
	win \
	xlib

all-am:
	cd unix && $(MAKE) $(AM_MAKEFLAGS) all

clean-am:
	cd unix && $(MAKE) $(AM_MAKEFLAGS) clean

noprod:

prodclean-recursive:

prodclean:

fast: all-am

depends: all-am

DISTCLEANFILES = \
	unix/autom4te.cache \
	unix/config.log \
	unix/config.status \
	unix/configure \
	unix/tkConfig.sh \
	unix/wish

MAINTAINERCLEANFILES = Makefile.in
