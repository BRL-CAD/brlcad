
SUBDIRS = clock

bin_PROGRAMS = \
	applymap \
	avg4 \
	crop \
	fant \
	into \
	mcut \
	mergechan \
	pyrmask \
	repos \
	rleaddcom \
	rlebg \
	rlebox \
	rlecat \
	rleccube \
	rlecomp \
	rledither \
	rleflip \
	rlegrid \
	rlehdr \
	rlehisto \
	rleinterp \
	rleldmap \
	rlemandl \
	rlenoise \
	rlepatch \
	rleprint \
	rlequant \
	rlescale \
	rleselect \
	rlesetbg \
	rlesortmap \
	rlespiff \
	rlesplice \
	rlesplit \
	rlestereo \
	rleswap \
	rlezoom \
	smush \
	to8 \
	tobw \
	unexp \
	unslice

LDADD = ${RLE}
AM_CPPFLAGS = ${RLE_CPPFLAGS} ${NOWARN}

applymap_SOURCES = applymap.c
avg4_SOURCES = avg4.c
crop_SOURCES = crop.c rle_box.c
fant_SOURCES = fant.c mallocNd.c
into_SOURCES = into.c
mcut_SOURCES = mcut.c
mergechan_SOURCES = mergechan.c
pyrmask_SOURCES = pyrmask.c pyrlib.c
repos_SOURCES = repos.c
rleaddcom_SOURCES = rleaddcom.c
rlebg_SOURCES = rlebg.c
rlebox_SOURCES = rlebox.c rle_box.c
rlecat_SOURCES = rlecat.c
rleccube_SOURCES = rleccube.c
rlecomp_SOURCES = rlecomp.c
rledither_SOURCES = rledither.c
rleflip_SOURCES = rleflip.c
rlegrid_SOURCES = rlegrid.c
rlehdr_SOURCES = rlehdr.c
rlehisto_SOURCES = rlehisto.c
rleinterp_SOURCES = rleinterp.c
rleldmap_SOURCES = rleldmap.c
rlemandl_SOURCES = rlemandl.c
rlenoise_SOURCES = rlenoise.c
rlepatch_SOURCES = rlepatch.c
rleprint_SOURCES = rleprint.c
rlequant_SOURCES = rlequant.c
rlescale_SOURCES = rlescale.c
rleselect_SOURCES = rleselect.c
rlesetbg_SOURCES = rlesetbg.c
rlesortmap_SOURCES = rlesortmap.c
rlespiff_SOURCES = rlespiff.c
rlesplice_SOURCES = rlesplice.c
rlesplit_SOURCES = rlesplit.c
rlestereo_SOURCES = rlestereo.c
rleswap_SOURCES = rleswap.c
rlezoom_SOURCES = rlezoom.c
smush_SOURCES = smush.c
to8_SOURCES = to8.c
tobw_SOURCES = tobw.c
unexp_SOURCES = unexp.c
unslice_SOURCES = unslice.c

EXTRA_DIST = \
	rleaddeof.c \
	rleskel.c \
	CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(applymap_OBJECTS) \
	$(avg4_OBJECTS) \
	$(crop_OBJECTS) \
	$(fant_OBJECTS) \
	$(into_OBJECTS) \
	$(mcut_OBJECTS) \
	$(mergechan_OBJECTS) \
	$(pyrmask_OBJECTS) \
	$(repos_OBJECTS) \
	$(rleaddcom_OBJECTS) \
	$(rlebg_OBJECTS) \
	$(rlebox_OBJECTS) \
	$(rlecat_OBJECTS) \
	$(rlecomp_OBJECTS) \
	$(rledither_OBJECTS) \
	$(rleflip_OBJECTS) \
	$(rlegrid_OBJECTS) \
	$(rlehdr_OBJECTS) \
	$(rlehisto_OBJECTS) \
	$(rleinterp_OBJECTS) \
	$(rleldmap_OBJECTS) \
	$(rlemandl_OBJECTS) \
	$(rlenoise_OBJECTS) \
	$(rlepatch_OBJECTS) \
	$(rleprint_OBJECTS) \
	$(rlequant_OBJECTS) \
	$(rlescale_OBJECTS) \
	$(rleselect_OBJECTS) \
	$(rlesetbg_OBJECTS) \
	$(rlesortmap_OBJECTS) \
	$(rlespiff_OBJECTS) \
	$(rlesplice_OBJECTS) \
	$(rlesplit_OBJECTS) \
	$(rlestereo_OBJECTS) \
	$(rleswap_OBJECTS) \
	$(rlezoom_OBJECTS) \
	$(smush_OBJECTS) \
	$(to8_OBJECTS) \
	$(tobw_OBJECTS) \
	$(unexp_OBJECTS) \
	$(unslice_OBJECTS) \
	$(bin_PROGRAMS)
