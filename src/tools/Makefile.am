# $Id$

bin_PROGRAMS = \
	applymap \
	avg4 \
	crop \
	cubitorle \
	fant \
	giftorle \
	graytorle \
	into \
	mcut \
	mergechan \
	painttorle \
	pyrmask \
	rawtorle \
	repos \
	rleClock \
	rleaddcom \
	rlebg \
	rlebox \
	rlecat \
	rlecomp \
	rledither \
	rleflip \
	rlehdr \
	rlehisto \
	rleldmap \
	rlemandl \
	rlenoise \
	rlepatch \
	rleprint \
	rlequant \
	rlescale \
	rleselect \
	rlesetbg \
	rlespiff \
	rlesplice \
	rlesplit \
	rlestereo \
	rleswap \
	rletoabA60 \
	rletoabA62 \
	rletoascii \
	rletogif \
	rletogray \
	rletopaint \
	rletops \
	rletoraw \
	rlezoom \
	smush \
	targatorle \
	to8 \
	tobw \
	unexp \
	unslice \
	wasatchrle

bin_SCRIPTS = font.c
BUILT_SOURCES = font.c

LDADD = ${RLE} ${BU}
noinst_HEADERS = font.h rletogif.h

applymap_SOURCES = applymap.c
avg4_SOURCES = avg4.c
crop_SOURCES = crop.c rle_box.c
cubitorle_SOURCES = cubitorle.c
fant_SOURCES = fant.c
giftorle_SOURCES = giftorle.c
graytorle_SOURCES = graytorle.c
into_SOURCES = into.c
mcut_SOURCES = mcut.c
mergechan_SOURCES = mergechan.c
painttorle_SOURCES = painttorle.c
pyrmask_SOURCES = pyrmask.c pyrlib.c
rawtorle_SOURCES = rawtorle.c
repos_SOURCES = repos.c
rleClock_SOURCES = rleClock.c font.c
rleaddcom_SOURCES = rleaddcom.c
rlebg_SOURCES = rlebg.c
rlebox_SOURCES = rlebox.c rle_box.c
rlecat_SOURCES = rlecat.c
rlecomp_SOURCES = rlecomp.c
rledither_SOURCES = rledither.c
rleflip_SOURCES = rleflip.c
rlehdr_SOURCES = rlehdr.c
rlehisto_SOURCES = rlehisto.c
rleldmap_SOURCES = rleldmap.c
rlemandl_SOURCES = rlemandl.c
rlenoise_SOURCES = rlenoise.c
rlepatch_SOURCES = rlepatch.c
rleprint_SOURCES = rleprint.c
rlequant_SOURCES = rlequant.c
rlescale_SOURCES = rlescale.c
rleselect_SOURCES = rleselect.c
rlesetbg_SOURCES = rlesetbg.c
rlespiff_SOURCES = rlespiff.c
rlesplice_SOURCES = rlesplice.c
rlesplit_SOURCES = rlesplit.c
rlestereo_SOURCES = rlestereo.c
rleswap_SOURCES = rleswap.c
rletoabA60_SOURCES = rletoabA60.c
rletoabA62_SOURCES = rletoabA62.c rle.c
rletoascii_SOURCES = rletoascii.c
rletogif_SOURCES = rletogif.c gifencod.c compgif.c
rletogray_SOURCES = rletogray.c
rletopaint_SOURCES = rletopaint.c
rletops_SOURCES = rletops.c
rletoraw_SOURCES = rletoraw.c
rlezoom_SOURCES = rlezoom.c
smush_SOURCES = smush.c
targatorle_SOURCES = targatorle.c
to8_SOURCES = to8.c
tobw_SOURCES = tobw.c
unexp_SOURCES = unexp.c
unslice_SOURCES = unslice.c
wasatchrle_SOURCES = wasatchrle.c

font.c: ${srcdir}/makeFont.sh ${srcdir}/font.src
	@SH@ ${srcdir}/makeFont.sh ${srcdir}/font.src > font.c

man_MANS = \
	applymap.1 \
	avg4.1 \
	crop.1 \
	cubitorle.1 \
	fant.1 \
	giftorle.1 \
	graytorle.1 \
	into.1 \
	mcut.1 \
	mergechan.1 \
	painttorle.1 \
	pyrmask.1 \
	rawtorle.1 \
	repos.1 \
	rleClock.1 \
	rleaddcom.1 \
	rleaddeof.1 \
	rlebg.1 \
	rlebox.1 \
	rlecat.1 \
	rlecomp.1 \
	rledither.1 \
	rleflip.1 \
	rlehdr.1 \
	rlehisto.1 \
	rleldmap.1 \
	rlemandl.1 \
	rlenoise.1 \
	rlepatch.1 \
	rleprint.1 \
	rlequant.1 \
	rlescale.1 \
	rleselect.1 \
	rlesetbg.1 \
	rleskel.1 \
	rlespiff.1 \
	rlesplice.1 \
	rlesplit.1 \
	rlestereo.1 \
	rleswap.1 \
	rletoabA60.1 \
	rletoabA62.1 \
	rletoascii.1 \
	rletogif.1 \
	rletogray.1 \
	rletopaint.1 \
	rletops.1 \
	rletoraw.1 \
	rlezoom.1 \
	smush.1 \
	targatorle.1 \
	to8.1 \
	tobw.1 \
	unexp.1 \
	unslice.1 \
	urt.1 \
	wasatchrle.1

EXTRA_DIST = \
	$(man_MANS) \
	font.src \
	makeFont.sh \
	rleaddeof.c \
	rleskel.c

DISTCLEANFILES = font.c
MAINTAINERCLEANFILES = Makefile.in
