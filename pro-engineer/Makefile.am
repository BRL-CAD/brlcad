# $Id$

SUBDIRS = sgi_mips4 text resource

stuffdir = $(prefix)/pro-engineer
stuff_DATA = install.doc protk.dat
EXTRA_DIST = install.doc protk.in

protk.dat: protk.in Makefile.am
	sed -e 's&/usr/brlcad&BASEDIR&' < protk.in > protk.dat

