# $Id$

bin_PROGRAMS = \
	canonize \
	pix-ipu \
	png-ipu \
	ipuscan \
	ipustat

noinst_HEADERS = \
	canon.h \
	chore.h

man_MANS = canonize.1


canonize_SOURCES = canonize.c canonlib.c asize.c

canonize_LDADD = ${LIBM}

pix_ipu_SOURCES = pix-ipu.c canonlib.c asize.c

png_ipu_SOURCES = png-ipu.c canonlib.c asize.c

ipuscan_SOURCES = ipuscan.c canonlib.c asize.c

ipustat_SOURCES = ipustat.c canonlib.c

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = $(man_MANS)
