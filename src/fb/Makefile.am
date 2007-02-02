# $Id$

bin_PROGRAMS = \
	bw-fb \
	cat-fb \
	cell-fb \
	cmap-fb \
	fb-bw \
	fb-cmap \
	fb-fb \
	fb-orle \
	fb-pix \
	fb-png \
	fb-rle \
	fbanim \
	fbcbars \
	fbclear \
	fbcmap \
	fbcmrot \
	fbcolor \
	fbfade \
	fbframe \
	fbfree \
	fbgamma \
	fbgammamod \
	fbgrid \
	fbhelp \
	fblabel \
	fbline \
	fbpoint \
	fbscanplot \
	fbstretch \
	fbzoom \
	gif-fb \
	gif2fb \
	orle-fb \
	pix-fb \
	pixautosize \
	pixflip-fb \
	pl-fb \
	png-fb \
	polar-fb \
	pp-fb \
	rle-fb \
	spm-fb

fbanim_SOURCES = fbanim.c
fbanim_LDADD = ${FB}

pl_fb_SOURCES = pl-fb.c
pl_fb_LDADD = ${FB} ${BN}

fbcbars_SOURCES = fbcbars.c
fbcbars_LDADD = ${FB}

png_fb_SOURCES = png-fb.c
png_fb_LDADD = ${FB} ${PNG}
# per-program CPPFLAGS wasn't added until automake 1.7
png_fb_CFLAGS = ${PNG_CPPFLAGS}

fb_png_SOURCES = fb-png.c cmap-crunch.c
fb_png_LDADD = ${FB} ${PNG}
# per-program CPPFLAGS wasn't added until automake 1.7
fb_png_CFLAGS = ${PNG_CPPFLAGS}

pix_fb_SOURCES = pix-fb.c
pix_fb_LDADD = ${FB} ${BN} ${BU}

fb_pix_SOURCES = fb-pix.c cmap-crunch.c
fb_pix_LDADD = ${FB}

fb_fb_SOURCES = fb-fb.c
fb_fb_LDADD = ${FB}

pixflip_fb_SOURCES = pixflip-fb.c
pixflip_fb_LDADD = ${FB}

pp_fb_SOURCES = pp-fb.c
pp_fb_LDADD = ${FB} ${TERMIO}

fbclear_SOURCES = fbclear.c
fbclear_LDADD = ${FB}

fbfree_SOURCES = fbfree.c
fbfree_LDADD = ${FB}

fbhelp_SOURCES = fbhelp.c
fbhelp_LDADD = ${FB}

fbgrid_SOURCES = fbgrid.c
fbgrid_LDADD = ${FB}

pixautosize_SOURCES = pixautosize.c
pixautosize_LDADD = ${FB} ${BN}

orle_fb_SOURCES = orle-fb.c
orle_fb_LDADD = ${FB} ${ORLE}

fb_orle_SOURCES = fb-orle.c cmap-crunch.c
fb_orle_LDADD = ${FB} ${ORLE}

rle_fb_SOURCES = rle-fb.c
rle_fb_LDADD = ${FB} ${RLE}

fb_rle_SOURCES = fb-rle.c cmap-crunch.c
fb_rle_LDADD = ${FB} ${RLE}

fbcmap_SOURCES = fbcmap.c
fbcmap_LDADD = ${FB}

fb_cmap_SOURCES = fb-cmap.c
fb_cmap_LDADD = ${FB}

cmap_fb_SOURCES = cmap-fb.c
cmap_fb_LDADD = ${FB}

fbgamma_SOURCES = fbgamma.c
fbgamma_LDADD = ${FB}

fbgammamod_SOURCES = fbgammamod.c
fbgammamod_LDADD = ${FB}

fbscanplot_SOURCES = fbscanplot.c
fbscanplot_LDADD = ${FB}

fbzoom_SOURCES = fbzoom.c
fbzoom_LDADD = ${FB} ${TERMIO}

fbpoint_SOURCES = fbpoint.c
fbpoint_LDADD = ${FB} ${TERMIO}

fblabel_SOURCES = fblabel.c
fblabel_LDADD = ${FB}

fbline_SOURCES = fbline.c
fbline_LDADD = ${FB}

fbcolor_SOURCES = fbcolor.c
fbcolor_LDADD = ${FB} ${TERMIO}

fbcmrot_SOURCES = fbcmrot.c
fbcmrot_LDADD = ${FB}

fbframe_SOURCES = fbframe.c
fbframe_LDADD = ${FB}

bw_fb_SOURCES = bw-fb.c
bw_fb_LDADD = ${FB} ${BN} ${BU}

fb_bw_SOURCES = fb-bw.c
fb_bw_LDADD = ${FB}

cat_fb_SOURCES = cat-fb.c
cat_fb_LDADD = ${FB} ${BU}

gif2fb_SOURCES = gif2fb.c
gif2fb_LDADD = ${FB}

fbfade_SOURCES = fbfade.c
fbfade_LDADD = ${FB}

fbstretch_SOURCES = fbstretch.c
fbstretch_LDADD = ${FB}

gif_fb_SOURCES = gif-fb.c
gif_fb_LDADD = ${FB}

spm_fb_SOURCES = spm-fb.c
spm_fb_LDADD = ${RT} ${FB} ${BN}

polar_fb_SOURCES = polar-fb.c
polar_fb_LDADD = ${FB}

cell_fb_SOURCES = cell-fb.c
cell_fb_LDADD = ${FB} ${BN}

man_MANS = \
	bw-fb.1 \
	cat-fb.1 \
	cell-fb.1 \
	cmap-fb.1 \
	fb-bw.1 \
	fb-cmap.1 \
	fb-fb.1 \
	fb-orle.1 \
	fb-pix.1 \
	fb-png.1 \
	fb-rle.1 \
	fbanim.1 \
	fbcbars.1 \
	fbclear.1 \
	fbcmap.1 \
	fbcmrot.1 \
	fbcolor.1 \
	fbfade.1 \
	fbframe.1 \
	fbfree.1 \
	fbgamma.1 \
	fbgrid.1 \
	fbhelp.1 \
	fblabel.1 \
	fbline.1 \
	fbpoint.1 \
	fbscanplot.1 \
	fbstretch.1 \
	fbzoom.1 \
	gif-fb.1 \
	gif2fb.1 \
	orle-fb.1 \
	pix-fb.1 \
	pixautosize.1 \
	pl-fb.1 \
	png-fb.1 \
	polar-fb.1 \
	pp-fb.1 \
	rle-fb.1

EXTRA_DIST = $(man_MANS)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bw_fb_OBJECTS) \
	$(cat_fb_OBJECTS) \
	$(cell_fb_OBJECTS) \
	$(cmap_fb_OBJECTS) \
	$(fb_bw_OBJECTS) \
	$(fb_cmap_OBJECTS) \
	$(fb_fb_OBJECTS) \
	$(fb_orle_OBJECTS) \
	$(fb_pix_OBJECTS) \
	$(fb_png_OBJECTS) \
	$(fb_rle_OBJECTS) \
	$(fbanim_OBJECTS) \
	$(fbcbars_OBJECTS) \
	$(fbclear_OBJECTS) \
	$(fbcmap_OBJECTS) \
	$(fbcmrot_OBJECTS) \
	$(fbcolor_OBJECTS) \
	$(fbfade_OBJECTS) \
	$(fbframe_OBJECTS) \
	$(fbfree_OBJECTS) \
	$(fbgamma_OBJECTS) \
	$(fbgammamod_OBJECTS) \
	$(fbgrid_OBJECTS) \
	$(fbhelp_OBJECTS) \
	$(fblabel_OBJECTS) \
	$(fbline_OBJECTS) \
	$(fbpoint_OBJECTS) \
	$(fbscanplot_OBJECTS) \
	$(fbstretch_OBJECTS) \
	$(fbzoom_OBJECTS) \
	$(gif_fb_OBJECTS) \
	$(gif2fb_OBJECTS) \
	$(orle_fb_OBJECTS) \
	$(pix_fb_OBJECTS) \
	$(pixautosize_OBJECTS) \
	$(pixflip_fb_OBJECTS) \
	$(pl_fb_OBJECTS) \
	$(png_fb_OBJECTS) \
	$(polar_fb_OBJECTS) \
	$(pp_fb_OBJECTS) \
	$(rle_fb_OBJECTS) \
	$(spm_fb_OBJECTS) \
	$(bin_PROGRAMS)
