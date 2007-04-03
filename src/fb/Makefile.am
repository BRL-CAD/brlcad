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

AM_CPPFLAGS = ${TCL_CPPFLAGS}

bw_fb_LDADD = ${FB} ${FB_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
bw_fb_SOURCES = bw-fb.c

cat_fb_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
cat_fb_SOURCES = cat-fb.c

cell_fb_LDADD = ${FB} ${FB_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
cell_fb_SOURCES = cell-fb.c

cmap_fb_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
cmap_fb_SOURCES = cmap-fb.c

fb_bw_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fb_bw_SOURCES = fb-bw.c

fb_cmap_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fb_cmap_SOURCES = fb-cmap.c

fb_fb_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fb_fb_SOURCES = fb-fb.c

fb_orle_LDADD = ${FB} ${FB_LIBS} ${ORLE} ${ORLE_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fb_orle_SOURCES = fb-orle.c cmap-crunch.c

fb_pix_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS} ${PKG} ${PKG_LIBS}
fb_pix_SOURCES = fb-pix.c cmap-crunch.c

# per-program CPPFLAGS wasn't added until automake 1.7
fb_png_CFLAGS = ${PNG_CPPFLAGS}
fb_png_LDADD = ${FB} ${FB_LIBS} ${PNG} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fb_png_SOURCES = fb-png.c cmap-crunch.c

# per-program CPPFLAGS wasn't added until automake 1.7
fb_rle_CFLAGS = ${RLE_CPPFLAGS}
fb_rle_LDADD = ${FB} ${FB_LIBS} ${RLE} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fb_rle_SOURCES = fb-rle.c cmap-crunch.c

fbanim_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbanim_SOURCES = fbanim.c

fbcbars_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbcbars_SOURCES = fbcbars.c

fbclear_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbclear_SOURCES = fbclear.c

fbcmap_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbcmap_SOURCES = fbcmap.c

fbcmrot_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbcmrot_SOURCES = fbcmrot.c

fbcolor_LDADD = ${FB} ${FB_LIBS} ${TERMIO} ${TERMIO_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbcolor_SOURCES = fbcolor.c

fbfade_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbfade_SOURCES = fbfade.c

fbframe_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbframe_SOURCES = fbframe.c

fbfree_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbfree_SOURCES = fbfree.c

fbgamma_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbgamma_SOURCES = fbgamma.c

fbgammamod_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbgammamod_SOURCES = fbgammamod.c

fbgrid_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbgrid_SOURCES = fbgrid.c

fbhelp_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbhelp_SOURCES = fbhelp.c

fblabel_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fblabel_SOURCES = fblabel.c

fbline_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbline_SOURCES = fbline.c

fbpoint_LDADD = ${FB} ${FB_LIBS} ${TERMIO} ${TERMIO_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbpoint_SOURCES = fbpoint.c

fbscanplot_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbscanplot_SOURCES = fbscanplot.c

fbstretch_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbstretch_SOURCES = fbstretch.c

fbzoom_LDADD = ${FB} ${FB_LIBS} ${TERMIO} ${TERMIO_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
fbzoom_SOURCES = fbzoom.c

gif2fb_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
gif2fb_SOURCES = gif2fb.c

gif_fb_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
gif_fb_SOURCES = gif-fb.c

orle_fb_LDADD = ${FB} ${FB_LIBS} ${ORLE} ${ORLE_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
orle_fb_SOURCES = orle-fb.c

pix_fb_LDADD = ${FB} ${FB_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
pix_fb_SOURCES = pix-fb.c

pixautosize_LDADD = ${FB} ${FB_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
pixautosize_SOURCES = pixautosize.c

pixflip_fb_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
pixflip_fb_SOURCES = pixflip-fb.c

pl_fb_LDADD = ${FB} ${FB_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
pl_fb_SOURCES = pl-fb.c

# per-program CPPFLAGS wasn't added until automake 1.7
png_fb_CFLAGS = ${PNG_CPPFLAGS}
png_fb_LDADD = ${FB} ${FB_LIBS} ${PNG} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
png_fb_SOURCES = png-fb.c

polar_fb_LDADD = ${FB} ${FB_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
polar_fb_SOURCES = polar-fb.c

pp_fb_LDADD = ${FB} ${FB_LIBS} ${TERMIO} ${TERMIO_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
pp_fb_SOURCES = pp-fb.c

# per-program CPPFLAGS wasn't added until automake 1.7
rle_fb_CFLAGS = ${RLE_CPPFLAGS}
rle_fb_LDADD = ${FB} ${FB_LIBS} ${RLE} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
rle_fb_SOURCES = rle-fb.c

spm_fb_LDADD = ${RT} ${RT_LIBS} ${FB} ${FB_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS}
spm_fb_SOURCES = spm-fb.c

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
