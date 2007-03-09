# $Id$

EXTRA_DIST = \
	ChangeLog \
	ChangeLog.1999 \
	ChangeLog.2000 \
	ChangeLog.2001 \
	ChangeLog.2002 \
	ChangeLog.2003 \
	ChangeLog.2004 \
	README \
	changes \
	compat \
	doc \
	generic \
	library \
	libtommath \
	license.terms \
	macosx \
	tests \
	tools \
	unix \
	win

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
	unix/tclConfig.sh \
	unix/tclsh

MAINTAINERCLEANFILES = Makefile.in
