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

pix_ipu_SOURCES = pix-ipu.c canonlib.c asize.c mread.c
pix_ipu_LDADD = ${LIBM}

png_ipu_SOURCES = png-ipu.c canonlib.c asize.c mread.c
png_ipu_LDADD = ${LIBM}

ipuscan_SOURCES = ipuscan.c canonlib.c asize.c
ipuscan_LDADD = ${LIBM}

ipustat_SOURCES = ipustat.c canonlib.c

EXTRA_DIST = $(man_MANS)

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(canonize_OBJECTS) \
	$(canonserver_OBJECTS) \
	$(pix_ipu_OBJECTS) \
	$(png_ipu_OBJECTS) \
	$(ipuscan_OBJECTS) \
	$(ipustat_OBJECTS) \
	$(bin_PROGRAMS)
