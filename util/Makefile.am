# $Id$
bin_PROGRAMS= alias-pix ap-pix asc-pl azel bary buffer dbcp \
	binfo \
	bw-a bw-imp bw-pix bw-ps bw3-pix bw-png png-bw png_info \
	bwcrop bwdiff bwfilter bwhist bwhisteq \
	bwmod bwrect bwrot bwscale bwshrink \
	bwstat bwthresh \
	cv double-asc dpix-pix dunncolor dunnsnap \
	pixelswap decimate \
	files-tape gencolor hd loop mac-pix \
	op-bw orle-pix \
	pix-alias pix-bw pix-bw3 pix-orle \
	pix-ps pix-rle bw-rle pix-spm pix-sun \
	pix-ppm pcd-pix pix-png png-pix \
	pix-yuv yuv-pix \
	pix3filter pixbackgnd pixbgstrip pixbustup pixcolors pixdiff \
	pixdsplit pixfade pixfields pixfieldsep pixfilter pixhalve pixhist \
	pixhist3d-pl pixhist3d pixinterp2x pixmatte pixmerge \
	pixrect pixrot pixsaturate pixscale pixshrink pixstat \
	pixtile pixuntile pixembed \
	pixcut pixpaste \
	pl-asc pl-hpgl \
	pl-pl pl-tek pl-ps pl-sgi \
	plcolor pldebug plgetframe plline2 plrot plstat \
	query rle-pix sgi-pix sun-pix xyz-pl msrandom pixblend pixmorph \
	pixborder mst texturescale pixclump pixcount imgdims remapid \
	wavelet pixsubst terrain ttcp dsp_add \
	pl-X

dist_bin_SCRIPTS = rtwizard morphedit.tcl

pixsubst_SOURCES = pixsubst.c

wavelet_SOURCES = wavelet.c
wavelet_LDADD = @BN@ @FB@

png_info_SOURCES = png_info.c
png_info_LDADD = @BN@ @PNG_LIBS@

pix_png_SOURCES = pix-png.c
pix_png_LDADD = @BN@ @PNG_LIBS@

png_pix_SOURCES = png-pix.c
png_pix_LDADD = @BN@ @PNG_LIBS@

bw_png_SOURCES = bw-png.c
bw_png_LDADD = @BN@ @PNG_LIBS@

png_bw_SOURCES = png-bw.c
png_bw_LDADD = @BN@ @PNG_LIBS@

dunncolor_SOURCES = dunncolor.c dunncomm.c

dunnsnap_SOURCES = dunnsnap.c dunncomm.c
dunnsnap_LDADD = @FB@

loop_SOURCES = loop.c

mac_pix_SOURCES = mac-pix.c

sun_pix_SOURCES = sun-pix.c

ttcp_SOURCES = ttcp.c

pixmerge_SOURCES = pixmerge.c

pixmatte_SOURCES = pixmatte.c

pixbustup_SOURCES = pixbustup.c

pixfade_SOURCES = pixfade.c
pixfade_LDADD = @BN@

pixfields_SOURCES = pixfields.c

pixfieldsep_SOURCES = pixfieldsep.c

pixdiff_SOURCES = pixdiff.c

pixinterp2x_SOURCES = pixinterp2x.c

pixtile_SOURCES = pixtile.c

pixuntile_SOURCES = pixuntile.c

orle_pix_SOURCES = orle-pix.c
orle_pix_LDADD = @FB@ @ORLE@

pix_orle_SOURCES = pix-orle.c
pix_orle_LDADD = @FB@ @ORLE@

rle_pix_SOURCES = rle-pix.c strdup.c
rle_pix_LDADD = @RLE@

pix_rle_SOURCES = pix-rle.c strdup.c
pix_rle_LDADD = @RLE@

bw_rle_SOURCES = bw-rle.c strdup.c
bw_rle_LDADD = @RLE@

ap_pix_SOURCES = ap-pix.c

pixbackgnd_SOURCES = pixbackgnd.c

pixbgstrip_SOURCES = pixbgstrip.c
pixbgstrip_LDADD = @BN@

bw_a_SOURCES = bw-a.c
bw_a_LDADD = @BN@

bw_imp_SOURCES = bw-imp.c

bw_ps_SOURCES = bw-ps.c

bw_pix_SOURCES = bw-pix.c

bw3_pix_SOURCES = bw3-pix.c

bwcrop_SOURCES = bwcrop.c

bwdiff_SOURCES = bwdiff.c

bwfilter_SOURCES = bwfilter.c

bwhist_SOURCES = bwhist.c
bwhist_LDADD = @FB@

bwhisteq_SOURCES = bwhisteq.c

bwmod_SOURCES = bwmod.c
bwmod_LDADD = -lm

bwrect_SOURCES = bwrect.c

bwrot_SOURCES = bwrot.c
bwrot_LDADD = -lm

bwscale_SOURCES = bwscale.c

bwstat_SOURCES = bwstat.c
bwstat_LDADD = -lm

gencolor_SOURCES = gencolor.c

pix_bw_SOURCES = pix-bw.c

pix_bw3_SOURCES = pix-bw3.c

pixfilter_SOURCES = pixfilter.c

pixhist_SOURCES = pixhist.c
pixhist_LDADD = @FB@

pixhist3d_SOURCES = pixhist3d.c
pixhist3d_LDADD = @FB@

pixrect_SOURCES = pixrect.c

pixrot_SOURCES = pixrot.c

pixscale_SOURCES = pixscale.c

pixembed_SOURCES = pixembed.c

pixcut_SOURCES = pixcut.c
pixcut_LDADD = @BN@

pixpaste_SOURCES = pixpaste.c
pixpaste_LDADD = @BN@

pixstat_SOURCES = pixstat.c
pixstat_LDADD = -lm

pixhist3d_pl_SOURCES = pixhist3d-pl.c
pixhist3d_pl_LDADD = @BN@

pl_sgi_SOURCES = pl-sgi.c
pl_sgi_LDADD = @SGIGL@ @BU@

pl_ps_SOURCES = pl-ps.c
pl_ps_LDADD = @BU@

pldebug_SOURCES = pldebug.c
pldebug_LDADD = @BU@

plgetframe_SOURCES = plgetframe.c

plrot_SOURCES = plrot.c
plrot_LDADD = @BN@

cv_SOURCES = cv.c
cv_LDADD = @BU@

op_bw_SOURCES = op-bw.c

plcolor_SOURCES = plcolor.c
plcolor_LDADD = @BN@

plline2_SOURCES = plline2.c
plline2_LDADD = @BN@

pl_pl_SOURCES = pl-pl.c
pl_pl_LDADD = @BU@

pl_tek_SOURCES = pl-tek.c
pl_tek_LDADD = @BU@

buffer_SOURCES = buffer.c

dbcp_SOURCES = dbcp.c

files_tape_SOURCES = files-tape.c

pix3filter_SOURCES = pix3filter.c

pix_spm_SOURCES = pix-spm.c
pix_spm_LDADD = @BN@

pix_sun_SOURCES = pix-sun.c

pix_alias_SOURCES = pix-alias.c

alias_pix_SOURCES = alias-pix.c

pixcolors_SOURCES = pixcolors.c

hd_SOURCES = hd.c

sgi_pix_SOURCES = sgi-pix.c
sgi_pix_LDADD = @SGIGL@

pixsaturate_SOURCES = pixsaturate.c

query_SOURCES = query.c

pixshrink_SOURCES = pixshrink.c

bwshrink_SOURCES = bwshrink.c

pixhalve_SOURCES = pixhalve.c
pixhalve_LDADD = @BN@

pix_ps_SOURCES = pix-ps.c

pix_ppm_SOURCES = pix-ppm.c 
pix_ppm_LDADD = @BN@

pcd_pix_SOURCES = pcd-pix.c

pix_yuv_SOURCES = pix-yuv.c
pix_yuv_LDADD = @BN@

yuv_pix_SOURCES = yuv-pix.c
yuv_pix_LDADD = @BN@

plstat_SOURCES = plstat.c
plstat_LDADD = @BU@

pl_asc_SOURCES = pl-asc.c
pl_asc_LDADD = @BU@

asc_pl_SOURCES = asc-pl.c
asc_pl_LDADD = @BN@ -lm

pl_hpgl_SOURCES = pl-hpgl.c

xyz_pl_SOURCES = xyz-pl.c
xyz_pl_LDADD = @BN@

azel_SOURCES = azel.c
azel_LDADD = -lm

bary_SOURCES = bary.c
bary_LDADD = @BU@

pixdsplit_SOURCES = pixdsplit.c
pixdsplit_LDADD = @BU@

mst_SOURCES = mst.c
mst_LDADD = @BU@

remapid_SOURCES = remapid.c
remapid_LDADD = @RT@

texturescale_SOURCES = texturescale.c
texturescale_LDADD = @BN@

double_asc_SOURCES = double-asc.c
double_asc_LDADD = @BN@

bwthresh_SOURCES = bwthresh.c

dpix_pix_SOURCES = dpix-pix.c

#pix_ci_SOURCES = pix-ci.c
#pix_ci_LDADD = @X_LIBS@

pl_X_SOURCES = pl-X.c
pl_X_CFLAGS = @X_CFLAGS@
pl_X_LDADD = @X_LIBS@ @BU@

#pl_X10_SOURCES = pl-X10.c

#pl_starbase_SOURCES = pl-starbase.c

pixelswap_SOURCES = pixelswap.c

decimate_SOURCES = decimate.c

msrandom_SOURCES = msrandom.c
msrandom_LDADD = @BN@

pixblend_SOURCES = pixblend.c

pixmorph_SOURCES = pixmorph.c
pixmorph_LDADD = @BN@

pixborder_SOURCES = pixborder.c
pixborder_LDADD = @BN@

pixclump_SOURCES = pixclump.c
pixclump_LDADD = @BU@

pixcount_SOURCES = pixcount.c
pixcount_LDADD = @BU@

imgdims_SOURCES = imgdims.c
imgdims_LDADD = @BN@

terrain_SOURCES = terrain.c
terrain_LDADD = @BN@

dsp_add_SOURCES = dsp_add.c
dsp_add_LDADD = @BU@

binfo_SOURCES = binfo.c 
binfo_LDADD = @BN@ @FB@

rtwizard = $(top_srcdir)//rtwizard
	cp $(top_srcdir)/tclscripts/rtwizard rtwizard

AM_CFLAGS = -I$(top_srcdir)/h

.PHONY : vers.c
vers.c:
	$(top_srcdir)/sh/newvers.sh version "BRL-CAD info"

man_MANS = alias-pix.1 ap-pix.1 asc-pl.1 azel.1 bary.1 brlcad.1 buffer.1 bw-imp.1 bw-pix.1 bw-png.1 bw-ps.1 bw-rle.1 bw3-pix.1 bwcrop.1 bwdiff.1 bwfilter.1 bwhist.1 bwhisteq.1 bwmod.1 bwrect.1 bwrot.1 bwscale.1 bwshrink.1 bwstat.1 bwthresh.1 cv.1 dbcp.1 decimate.1 dpix-pix.1 dunncolor.1 dunnsnap.1 gencolor.1 hd.1 imgdims.1 loop.1 mac-pix.1 morphedit.1 op-bw.1 orle-pix.1 pcd-pix.1 pix-alias.1 pix-bw.1 pix-bw3.1 pix-orle.1 pix-png.1 pix-ps.1 pix-rle.1 pix-sun.1 pixbackgnd.1 pixbgstrip.1 pixblend.1 pixborder.1 pixbustup.1 pixclump.1 pixcolors.1 pixcut.1 pixdiff.1 pixdsplit.1 pixfade.1 pixfilter.1 pixhalve.1 pixhist.1 pixhist3d-pl.1 pixhist3d.1 pixinterp2x.1 pixmatte.1 pixmerge.1 pixmorph.1 pixrect.1 pixrot.1 pixsaturate.1 pixscale.1 pixshrink.1 pixstat.1 pixsubst.1 pixtile.1 pixuntile.1 pl-X.1 pl-asc.1 pl-hpgl.1 pl-pl.1 pl-ps.1 pl-sgi.1 pl-starbase.1 pl-tek.1 plcolor.1 pldebug.1 plgetframe.1 plline2.1 plrot.1 png-bw.1 png-pix.1 png_info.1 query.1 rle-pix.1 sun-pix.1 terrain.1 wavelet.1 pl-X10.1
EXTRA_DIST = $(man_MANS)

pl-X10.1: pl-X.1
	cp $< $@
