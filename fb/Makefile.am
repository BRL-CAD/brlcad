bin_PROGRAMS = bw-fb cat-fb cell-fb cmap-fb fbgammamod fb-bw fb-cmap fb-fb fb-orle fb-pix fb-rle fbanim fbcbars fbclear fbcmap fbcmrot fbcolor fbfade fbframe fbfree fbgamma fbgrid fbhelp fblabel fbline fbpoint fbscanplot fbstretch fbzoom gif-fb gif2fb orle-fb pix-fb pixflip-fb pl-fb polar-fb pp-fb rle-fb spm-fb pixautosize png-fb fb-png

BN=$(top_builddir)/libbn/libbn.la $(BU)
BU=$(top_builddir)/libbu/libbu.la @TCL_LIBS@ @PTHREAD_LIBS@ -lm 
FB=$(top_builddir)/libfb/libfb.la @X_LIBS@ $(PKG) $(BU)
PKG=$(top_builddir)/libpkg/libpkg.la
PNG=@PNG_LIBS@
SYSV=$(top_builddir)/libsysv/libsysv.la
ORLE=$(top_builddir)/liborle/liborle.la
RLE=$(top_builddir)/libutahrle/libutahrle.la -lm
RT=$(top_builddir)/librt/librt.la $(BN)
TERMIO=$(top_builddir)/libtermio/libtermio.la

fbanim_SOURCES = fbanim.c 
fbanim_LDADD = $(FB) $(BU)

pl_fb_SOURCES = pl-fb.c 
pl_fb_LDADD = $(FB) $(BU) $(BN)

fbcbars_SOURCES = fbcbars.c 
fbcbars_LDADD = $(FB) $(BU)

png_fb_SOURCES = png-fb.c 
png_fb_LDADD = $(FB) $(PNG) $(BU)

fb_png_SOURCES = fb-png.c cmap-crunch.c 
fb_png_LDADD = $(PNG) $(FB) $(BU)

pix_fb_SOURCES = pix-fb.c 
pix_fb_LDADD = $(FB) $(RT) $(BU) $(BN)

fb_pix_SOURCES = fb-pix.c cmap-crunch.c 
fb_pix_LDADD = $(FB) $(BU)

fb_fb_SOURCES = fb-fb.c 
fb_fb_LDADD = $(FB) $(BU)

pixflip_fb_SOURCES = pixflip-fb.c 
pixflip_fb_LDADD = $(FB) $(BU)

pp_fb_SOURCES = pp-fb.c 
pp_fb_LDADD = $(FB) $(TERMIO) $(BU)

fbclear_SOURCES = fbclear.c 
fbclear_LDADD = $(FB) $(BU)

fbfree_SOURCES = fbfree.c 
fbfree_LDADD = $(FB) $(BU)

fbhelp_SOURCES = fbhelp.c 
fbhelp_LDADD = $(FB) $(BU)

fbgrid_SOURCES = fbgrid.c 
fbgrid_LDADD = $(FB) $(BU)

pixautosize_SOURCES = pixautosize.c 
pixautosize_LDADD = $(FB) $(RT) $(BN) $(BU)

orle_fb_SOURCES = orle-fb.c 
orle_fb_LDADD = $(FB) $(ORLE) $(BU)

fb_orle_SOURCES = fb-orle.c cmap-crunch.c 
fb_orle_LDADD = $(FB) $(ORLE) $(BU)

rle_fb_SOURCES = rle-fb.c 
rle_fb_LDADD = $(FB) $(RLE) $(BU)

fb_rle_SOURCES = fb-rle.c cmap-crunch.c 
fb_rle_LDADD = $(FB) $(RLE) $(BU)

fbcmap_SOURCES = fbcmap.c 
fbcmap_LDADD = $(FB) $(BU)

fb_cmap_SOURCES = fb-cmap.c 
fb_cmap_LDADD = $(FB) $(BU)

cmap_fb_SOURCES = cmap-fb.c 
cmap_fb_LDADD = $(FB) $(BU)

fbgamma_SOURCES = fbgamma.c 
fbgamma_LDADD = $(FB) $(BU)

fbgammamod_SOURCES = fbgammamod.c 
fbgammamod_LDADD = $(FB) $(BU)

fbscanplot_SOURCES = fbscanplot.c 
fbscanplot_LDADD = $(FB) $(BU)

fbzoom_SOURCES = fbzoom.c 
fbzoom_LDADD = $(FB) $(TERMIO) $(BU)

fbpoint_SOURCES = fbpoint.c 
fbpoint_LDADD = $(FB) $(TERMIO) $(BU)

fblabel_SOURCES = fblabel.c 
fblabel_LDADD = $(FB) $(BU)

fbline_SOURCES = fbline.c 
fbline_LDADD = $(FB) $(BU)

fbcolor_SOURCES = fbcolor.c 
fbcolor_LDADD = $(FB) $(TERMIO)  $(BU)

fbcmrot_SOURCES = fbcmrot.c 
fbcmrot_LDADD = $(FB) $(BU)

fbframe_SOURCES = fbframe.c 
fbframe_LDADD = $(FB)  $(BU)

bw_fb_SOURCES = bw-fb.c 
bw_fb_LDADD = $(FB) $(RT) $(BU) $(BN)

fb_bw_SOURCES = fb-bw.c 
fb_bw_LDADD = $(FB) $(BU)

cat_fb_SOURCES = cat-fb.c 
cat_fb_LDADD = $(FB) $(BU)

gif2fb_SOURCES = gif2fb.c 
gif2fb_LDADD = $(FB) $(BU)

fbfade_SOURCES = fbfade.c 
fbfade_LDADD = $(FB) $(BU)

fbstretch_SOURCES = fbstretch.c 
fbstretch_LDADD = $(FB) $(BU)

gif_fb_SOURCES = gif-fb.c 
gif_fb_LDADD = $(FB) $(BU)

spm_fb_SOURCES = spm-fb.c 
spm_fb_LDADD = $(FB) $(BU) $(BN) $(RT)

polar_fb_SOURCES = polar-fb.c 
polar_fb_LDADD = $(FB) $(BU)

cell_fb_SOURCES = cell-fb.c 
cell_fb_LDADD = $(FB) $(BU) $(BN)


CFLAGS = -I$(top_srcdir)/h


man_MANS = bw-fb.1 cat-fb.1 cell-fb.1 cmap-fb.1 fb-bw.1 fb-cmap.1 fb-fb.1 fb-orle.1 fb-pix.1 fb-png.1 fb-rle.1 fbanim.1 fbcbars.1 fbclear.1 fbcmap.1 fbcmrot.1 fbcolor.1 fbfade.1 fbframe.1 fbfree.1 fbgamma.1 fbgrid.1 fbhelp.1 fblabel.1 fbline.1 fbpoint.1 fbscanplot.1 fbstretch.1 fbzoom.1 gif-fb.1 gif2fb.1 orle-fb.1 pix-fb.1 pixautosize.1 pl-fb.1 png-fb.1 polar-fb.1 pp-fb.1 rle-fb.1
EXTRA_DIST = $(man_MANS)
