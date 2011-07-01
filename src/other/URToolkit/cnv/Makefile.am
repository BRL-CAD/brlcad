
SUBDIRS = \
	rletoabA62 \
	rletogif

# Disabled due to various external dependancies:
#	iristorle
#	pgmtorle
#	ppmtorle
#	rastorle
#	rletocgm
#	rletoiris
#	rletoppm
#	rletorast
#	rletotiff
#	tifftorle

bin_PROGRAMS = \
	aliastorle \
	cubitorle \
	giftorle \
	graytorle \
	painttorle \
	rawtorle \
	rletoabA60 \
	rletoalias \
	rletoascii \
	rletogray \
	rletopaint \
	rletops \
	rletoraw \
	rletorla \
	rletotarga \
	rletovcr \
	targatorle \
	vcrtorle \
	wasatchrle \
	xbmtorle

noinst_HEADERS = \
	rla_header.h \
	rlb_header.h

LDADD = ${RLE}
AM_CPPFLAGS = ${RLE_CPPFLAGS} ${NOWARN}

aliastorle_SOURCES = aliastorle.c
cubitorle_SOURCES = cubitorle.c
giftorle_SOURCES = giftorle.c
graytorle_SOURCES = graytorle.c
# iristorle_SOURCES = iristorle.c
painttorle_SOURCES = painttorle.c
# pgmtorle_SOURCES = pgmtorle.c
# ppmtorle_SOURCES = ppmtorle.c
# rastorle_SOURCES = rastorle.c
rawtorle_SOURCES = rawtorle.c
rletoabA60_SOURCES = rletoabA60.c
rletoalias_SOURCES = rletoalias.c
rletoascii_SOURCES = rletoascii.c
# rletocgm_SOURCES = rletocgm.c
rletogray_SOURCES = rletogray.c
# rletoiris_SOURCES = rletoiris.c
rletopaint_SOURCES = rletopaint.c
# rletoppm_SOURCES = rletoppm.c
rletops_SOURCES = rletops.c
# rletorast_SOURCES = rletorast.c
rletoraw_SOURCES = rletoraw.c
rletorla_SOURCES = rletorla.c
rletotarga_SOURCES = rletotarga.c
# rletotiff_SOURCES = rletotiff.c
rletovcr_SOURCES = rletovcr.c
targatorle_SOURCES = targatorle.c
# tifftorle_SOURCES = tifftorle.c
vcrtorle_SOURCES = vcrtorle.c
wasatchrle_SOURCES = wasatchrle.c
xbmtorle_SOURCES = xbmtorle.c

EXTRA_DIST = \
	README \
	iristorle.c \
	pgmtorle.c \
	ppmtorle.c \
	rastorle.c \
	rlatorle.c \
	rletocgm.c \
	rletoiris.c \
	rletoppm.c \
	rletorast.c \
	rletotiff.c \
	tifftorle.c \
	CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(aliastorle_OBJECTS) \
	$(cubitorle_OBJECTS) \
	$(giftorle_OBJECTS) \
	$(graytorle_OBJECTS) \
	$(iristorle_OBJECTS) \
	$(painttorle_OBJECTS) \
	$(pgmtorle_OBJECTS) \
	$(ppmtorle_OBJECTS) \
	$(rastorle_OBJECTS) \
	$(rawtorle_OBJECTS) \
	$(rletoabA60_OBJECTS) \
	$(rletoalias_OBJECTS) \
	$(rletoascii_OBJECTS) \
	$(rletocgm_OBJECTS) \
	$(rletogray_OBJECTS) \
	$(rletoiris_OBJECTS) \
	$(rletopaint_OBJECTS) \
	$(rletoppm_OBJECTS) \
	$(rletops_OBJECTS) \
	$(rletorast_OBJECTS) \
	$(rletoraw_OBJECTS) \
	$(rletorla_OBJECTS) \
	$(rletotarga_OBJECTS) \
	$(rletotiff_OBJECTS) \
	$(rletovcr_OBJECTS) \
	$(targatorle_OBJECTS) \
	$(tifftorle_OBJECTS) \
	$(vcrtorle_OBJECTS) \
	$(wasatchrle_OBJECTS) \
	$(xbmtorle_OBJECTS) \
	$(bin_PROGRAMS)
