# $Id$

SUBDIRS = include

lib_LTLIBRARIES = libutahrle.la

libutahrle_la_SOURCES = \
	Runput.c \
	buildmap.c \
	cmd_name.c \
	colorquant.c \
	dither.c \
	float_to_exp.c \
	hilbert.c \
	inv_cmap.c \
	rle_addhist.c \
	rle_cp.c \
	rle_error.c \
	rle_getcom.c \
	rle_getraw.c \
	rle_getrow.c \
	rle_getskip.c \
	rle_global.c \
	rle_hdr.c \
	rle_open_f.c \
	rle_putcom.c \
	rle_putraw.c \
	rle_putrow.c \
	rle_raw_alc.c \
	rle_rawrow.c \
	rle_row_alc.c \
	scanargs.c \
	vaxshort.c

libutahrle_la_LDFLAGS = -version-info 19:1
libutahrle_la_CFLAGS = ${RLE_CPPFLAGS}
libutahrle_la_LIBADD = ${LIBM}

man_MANS = \
	buildmap.3 \
	bwdithermap.3 \
	colorquant.3 \
	dither.3 \
	ditherbw.3 \
	dithergb.3 \
	dithermap.3 \
	float_to_exp.3 \
	hilbert.3 \
	hilbert_c2i.3 \
	hilbert_i2c.3 \
	inv_cmap.3 \
	librle.3 \
	librle.3 \
	libutahrle.3 \
	make_square.3 \
	rgb_to_bw.3 \
	rle.5 \
	rle_addhist.3 \
	rle_cp.3 \
	rle_debug.3 \
	rle_delcom.3 \
	rle_freeraw.3 \
	rle_get_error.3 \
	rle_get_set.3 \
	rle_get_setup.3 \
	rle_get_setup_ok.3 \
	rle_getcom.3 \
	rle_getraw.3 \
	rle_getrow.3 \
	rle_getskip.3 \
	rle_hdr.3 \
	rle_op.3 \
	rle_open_f.3 \
	rle_open_f_noexit.3 \
	rle_put_init.3 \
	rle_put_setup.3 \
	rle_putcom.3 \
	rle_puteof.3 \
	rle_putraw.3 \
	rle_putrow.3 \
	rle_raw_alloc.3 \
	rle_raw_free.3 \
	rle_rawtorow.3 \
	rle_row_alloc.3 \
	rle_row_free.3 \
	rle_skiprow.3 \
	scanargs.3


EXTRA_DIST = \
	$(man_MANS) \
	getopt.c \
	include

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = $(libutahrle_la_OBJECTS)
