# $Id$

bin_PROGRAMS = \
	alias-pix \
	ap-pix \
	asc-pl \
	azel \
	bary \
	binfo \
	buffer \
	bw-a \
	bw-imp \
	bw-pix \
	bw-png \
	bw-ps \
	bw-rle \
	bw3-pix \
	bwcrop \
	bwdiff \
	bwfilter \
	bwhist \
	bwhisteq \
	bwmod \
	bwrect \
	bwrot \
	bwscale \
	bwshrink \
	bwstat \
	bwthresh \
	cv \
	dbcp \
	decimate \
	double-asc \
	dpix-pix \
	dsp_add \
	dunncolor \
	dunnsnap \
	files-tape \
	gencolor \
	hd \
	imgdims \
	loop \
	mac-pix \
	msrandom \
	mst \
	op-bw \
	orle-pix \
	pcd-pix \
	pix-alias \
	pix-bw \
	pix-bw3 \
	pix-orle \
	pix-png \
	pix-ppm \
	pix-ps \
	pix-rle \
	pix-spm \
	pix-sun \
	pix-yuv \
	pix3filter \
	pixbackgnd \
	pixbgstrip \
	pixblend \
	pixborder \
	pixbustup \
	pixclump \
	pixcolors \
	pixcount \
	pixcut \
	pixdiff \
	pixdsplit \
	pixelswap \
	pixembed \
	pixfade \
	pixfields \
	pixfieldsep \
	pixfilter \
	pixhalve \
	pixhist \
	pixhist3d \
	pixhist3d-pl \
	pixinterp2x \
	pixmatte \
	pixmerge \
	pixmorph \
	pixpaste \
	pixrect \
	pixrot \
	pixsaturate \
	pixscale \
	pixshrink \
	pixstat \
	pixsubst \
	pixtile \
	pixuntile \
	pl-X \
	pl-asc \
	pl-hpgl \
	pl-pl \
	pl-ps \
	pl-sgi \
	pl-tek \
	plcolor \
	pldebug \
	plgetframe \
	plline2 \
	plrot \
	plstat \
	png-bw \
	png-pix \
	png_info \
	query \
	remapid \
	rle-pix \
	sgi-pix \
	sun-pix \
	terrain \
	texturescale \
	ttcp \
	wavelet \
	xyz-pl \
	yuv-pix

# XXX - doesn't work, needs to be updated to newer dm interface
# noinst_PROGRAMS = pl-dm
noinst_PROGRAMS = roots_example

dist_bin_SCRIPTS = rtwizard morphedit.tcl


##########################################
# All the sources and flags sorted below #
##########################################

# XXX - example code doesn't work so disable for now
# pl_dm_LDADD = ${BU} ${DM}
# pl_dm_SOURCES = pl-dm.c

#pix_ci_LDADD = ${X_LIBS}
#pix_ci_SOURCES = pix-ci.c

#pl_X10_SOURCES = pl-X10.c
#pl_starbase_SOURCES = pl-starbase.c

alias_pix_LDADD = ${BU}
alias_pix_SOURCES = alias-pix.c
ap_pix_LDADD =
ap_pix_SOURCES = ap-pix.c
asc_pl_LDADD = ${BN} ${LIBM}
asc_pl_SOURCES = asc-pl.c
azel_LDADD = ${BU} ${LIBM}
azel_SOURCES = azel.c
bary_LDADD = ${BU}
bary_SOURCES = bary.c
binfo_LDADD = ${BN} ${FB}
binfo_SOURCES = binfo.c
buffer_LDADD = ${BU}
buffer_SOURCES = buffer.c
bw3_pix_LDADD = ${BU}
bw3_pix_SOURCES = bw3-pix.c
bw_a_LDADD = ${BN}
bw_a_SOURCES = bw-a.c
bw_imp_LDADD = ${BU}
bw_imp_SOURCES = bw-imp.c
bw_pix_LDADD = ${BU}
bw_pix_SOURCES = bw-pix.c
bw_png_LDADD = ${BN} ${PNG}
bw_png_SOURCES = bw-png.c
bw_ps_LDADD = ${BU}
bw_ps_SOURCES = bw-ps.c
bw_rle_LDADD = ${RLE} ${BU} ${SYSV}
bw_rle_SOURCES = bw-rle.c
bwcrop_LDADD = ${BU}
bwcrop_SOURCES = bwcrop.c
bwdiff_LDADD = ${BU}
bwdiff_SOURCES = bwdiff.c
bwfilter_LDADD = ${BU}
bwfilter_SOURCES = bwfilter.c
bwhist_LDADD = ${FB}
bwhist_SOURCES = bwhist.c
bwhisteq_LDADD = ${BU}
bwhisteq_SOURCES = bwhisteq.c
bwmod_LDADD = ${LIBM} ${BU}
bwmod_SOURCES = bwmod.c
bwrect_LDADD = ${BU}
bwrect_SOURCES = bwrect.c
bwrot_LDADD = ${LIBM} ${BU}
bwrot_SOURCES = bwrot.c
bwscale_LDADD = ${BU}
bwscale_SOURCES = bwscale.c
bwshrink_LDADD = ${BU}
bwshrink_SOURCES = bwshrink.c
bwstat_LDADD = ${LIBM}
bwstat_SOURCES = bwstat.c
bwthresh_LDADD = ${BU}
bwthresh_SOURCES = bwthresh.c
cv_LDADD = ${BU}
cv_SOURCES = cv.c
dbcp_LDADD = ${BU}
dbcp_SOURCES = dbcp.c
decimate_LDADD = ${BU}
decimate_SOURCES = decimate.c
double_asc_LDADD = ${BN}
double_asc_SOURCES = double-asc.c
dpix_pix_LDADD = ${BU}
dpix_pix_SOURCES = dpix-pix.c
dsp_add_LDADD = ${BU}
dsp_add_SOURCES = dsp_add.c
dunncolor_LDADD = ${BU}
dunncolor_SOURCES = dunncolor.c dunncomm.c
dunnsnap_LDADD = ${FB} ${BU}
dunnsnap_SOURCES = dunnsnap.c dunncomm.c
files_tape_LDADD = ${BU}
files_tape_SOURCES = files-tape.c
gencolor_LDADD = ${BU}
gencolor_SOURCES = gencolor.c
hd_LDADD = ${BU}
hd_SOURCES = hd.c
imgdims_LDADD = ${BN}
imgdims_SOURCES = imgdims.c
loop_LDADD = ${BU}
loop_SOURCES = loop.c
mac_pix_LDADD = ${BU}
mac_pix_SOURCES = mac-pix.c
msrandom_LDADD = ${BN}
msrandom_SOURCES = msrandom.c
mst_LDADD = ${BU}
mst_SOURCES = mst.c
op_bw_LDADD = ${BU}
op_bw_SOURCES = op-bw.c
orle_pix_LDADD = ${FB} ${ORLE}
orle_pix_SOURCES = orle-pix.c
pcd_pix_LDADD = ${BU}
pcd_pix_SOURCES = pcd-pix.c
pix3filter_LDADD = ${BU}
pix3filter_SOURCES = pix3filter.c
pix_alias_LDADD = ${BU}
pix_alias_SOURCES = pix-alias.c
pix_bw3_LDADD = ${BU}
pix_bw3_SOURCES = pix-bw3.c
pix_bw_LDADD = ${BU}
pix_bw_SOURCES = pix-bw.c
pix_orle_LDADD = ${FB} ${ORLE}
pix_orle_SOURCES = pix-orle.c
pix_png_LDADD = ${BN} ${PNG}
pix_png_SOURCES = pix-png.c
pix_ppm_LDADD = ${BN}
pix_ppm_SOURCES = pix-ppm.c
pix_ps_LDADD = ${BU}
pix_ps_SOURCES = pix-ps.c
pix_rle_LDADD = ${RLE} ${BU} ${SYSV}
pix_rle_SOURCES = pix-rle.c
pix_spm_LDADD = ${BN} ${BU}
pix_spm_SOURCES = pix-spm.c
pix_sun_LDADD = ${BU}
pix_sun_SOURCES = pix-sun.c
pix_yuv_LDADD = ${BN}
pix_yuv_SOURCES = pix-yuv.c
pixbackgnd_LDADD = ${BU}
pixbackgnd_SOURCES = pixbackgnd.c
pixbgstrip_LDADD = ${BN}
pixbgstrip_SOURCES = pixbgstrip.c
pixblend_LDADD = ${BU}
pixblend_SOURCES = pixblend.c
pixborder_LDADD = ${BN}
pixborder_SOURCES = pixborder.c
pixbustup_LDADD =
pixbustup_SOURCES = pixbustup.c
pixclump_LDADD = ${BU}
pixclump_SOURCES = pixclump.c
pixcolors_LDADD = ${BU}
pixcolors_SOURCES = pixcolors.c
pixcount_LDADD = ${BU}
pixcount_SOURCES = pixcount.c
pixcut_LDADD = ${BN} ${BU}
pixcut_SOURCES = pixcut.c
pixdiff_LDADD = ${BU}
pixdiff_SOURCES = pixdiff.c
pixdsplit_LDADD = ${BU}
pixdsplit_SOURCES = pixdsplit.c
pixelswap_LDADD = ${BU}
pixelswap_SOURCES = pixelswap.c
pixembed_LDADD = ${BU}
pixembed_SOURCES = pixembed.c
pixfade_LDADD = ${BN}
pixfade_SOURCES = pixfade.c
pixfields_LDADD = ${BU}
pixfields_SOURCES = pixfields.c
pixfieldsep_LDADD = ${BU}
pixfieldsep_SOURCES = pixfieldsep.c
pixfilter_LDADD = ${BU}
pixfilter_SOURCES = pixfilter.c
pixhalve_LDADD = ${BN}
pixhalve_SOURCES = pixhalve.c
pixhist3d_LDADD = ${FB} ${BU}
pixhist3d_SOURCES = pixhist3d.c
pixhist3d_pl_LDADD = ${BN} ${BU}
pixhist3d_pl_SOURCES = pixhist3d-pl.c
pixhist_LDADD = ${FB} ${BU}
pixhist_SOURCES = pixhist.c
pixinterp2x_LDADD = ${BU}
pixinterp2x_SOURCES = pixinterp2x.c
pixmatte_LDADD = ${BU}
pixmatte_SOURCES = pixmatte.c
pixmerge_LDADD = ${BU}
pixmerge_SOURCES = pixmerge.c
pixmorph_LDADD = ${BN}
pixmorph_SOURCES = pixmorph.c
pixpaste_LDADD = ${BN} ${BU}
pixpaste_SOURCES = pixpaste.c
pixrect_LDADD = ${BU}
pixrect_SOURCES = pixrect.c
pixrot_LDADD = ${BU}
pixrot_SOURCES = pixrot.c
pixsaturate_SOURCES = pixsaturate.c
pixscale_LDADD = ${BU}
pixscale_SOURCES = pixscale.c
pixshrink_LDADD = ${BU}
pixshrink_SOURCES = pixshrink.c
pixstat_LDADD = ${LIBM} ${BU}
pixstat_SOURCES = pixstat.c
pixsubst_LDADD = ${BU}
pixsubst_SOURCES = pixsubst.c
pixtile_LDADD = ${BU}
pixtile_SOURCES = pixtile.c
pixuntile_LDADD = ${BU}
pixuntile_SOURCES = pixuntile.c
pl_X_CFLAGS = ${X_CFLAGS}
pl_X_LDADD = ${X_LIBS} ${BU}
pl_X_SOURCES = pl-X.c
pl_asc_LDADD = ${BU}
pl_asc_SOURCES = pl-asc.c
pl_hpgl_LDADD = ${BU}
pl_hpgl_SOURCES = pl-hpgl.c
pl_pl_LDADD = ${BU}
pl_pl_SOURCES = pl-pl.c
pl_ps_LDADD = ${BU}
pl_ps_SOURCES = pl-ps.c
pl_sgi_LDADD = ${LIBSGIGL} ${BU}
pl_sgi_SOURCES = pl-sgi.c
pl_tek_LDADD = ${BU}
pl_tek_SOURCES = pl-tek.c
plcolor_LDADD = ${BN} ${BU}
plcolor_SOURCES = plcolor.c
pldebug_LDADD = ${BU}
pldebug_SOURCES = pldebug.c
plgetframe_LDADD = ${BU}
plgetframe_SOURCES = plgetframe.c
plline2_LDADD = ${BN} ${BU}
plline2_SOURCES = plline2.c
plrot_LDADD = ${BN}
plrot_SOURCES = plrot.c
plstat_LDADD = ${BU}
plstat_SOURCES = plstat.c
png_bw_LDADD = ${BN} ${PNG}
png_bw_SOURCES = png-bw.c
png_info_LDADD = ${BN} ${PNG}
png_info_SOURCES = png_info.c
png_pix_LDADD = ${BN} ${PNG}
png_pix_SOURCES = png-pix.c
query_LDADD = ${BU}
query_SOURCES = query.c
remapid_LDADD = ${LIBRT}
remapid_SOURCES = remapid.c
rle_pix_LDADD = ${RLE} ${BU} ${SYSV}
rle_pix_SOURCES = rle-pix.c
roots_example_LDADD = ${LIBRT} ${BN} ${BU}
roots_example_SOURCES = roots_example.c
sgi_pix_LDADD = ${LIBSGIGL}
sgi_pix_SOURCES = sgi-pix.c
sun_pix_LDADD = ${BU}
sun_pix_SOURCES = sun-pix.c
terrain_LDADD = ${BN}
terrain_SOURCES = terrain.c
texturescale_LDADD = ${BN}
texturescale_SOURCES = texturescale.c
ttcp_LDADD = ${LIBSOCKET} ${LIBNSL}
ttcp_SOURCES = ttcp.c
wavelet_LDADD = ${BN} ${FB}
wavelet_SOURCES = wavelet.c
xyz_pl_LDADD = ${BN}
xyz_pl_SOURCES = xyz-pl.c
yuv_pix_LDADD = ${BN} ${BU}
yuv_pix_SOURCES = yuv-pix.c

.PHONY : vers.c
vers.c:
	sh $(top_srcdir)/sh/vers.sh version "BRL-CAD info" > $@

man_MANS = \
	alias-pix.1 \
	ap-pix.1 \
	asc-pl.1 \
	azel.1 \
	bary.1 \
	brlcad.1 \
	buffer.1 \
	bw-imp.1 \
	bw-pix.1 \
	bw-png.1 \
	bw-ps.1 \
	bw-rle.1 \
	bw3-pix.1 \
	bwcrop.1 \
	bwdiff.1 \
	bwfilter.1 \
	bwhist.1 \
	bwhisteq.1 \
	bwmod.1 \
	bwrect.1 \
	bwrot.1 \
	bwscale.1 \
	bwshrink.1 \
	bwstat.1 \
	bwthresh.1 \
	cv.1 \
	dbcp.1 \
	decimate.1 \
	dpix-pix.1 \
	dunncolor.1 \
	dunnsnap.1 \
	gencolor.1 \
	hd.1 \
	imgdims.1 \
	loop.1 \
	mac-pix.1 \
	morphedit.1 \
	op-bw.1 \
	orle-pix.1 \
	pcd-pix.1 \
	pix-alias.1 \
	pix-bw.1 \
	pix-bw3.1 \
	pix-orle.1 \
	pix-png.1 \
	pix-ps.1 \
	pix-rle.1 \
	pix-sun.1 \
	pixbackgnd.1 \
	pixbgstrip.1 \
	pixblend.1 \
	pixborder.1 \
	pixbustup.1 \
	pixclump.1 \
	pixcolors.1 \
	pixcut.1 \
	pixdiff.1 \
	pixdsplit.1 \
	pixfade.1 \
	pixfilter.1 \
	pixhalve.1 \
	pixhist.1 \
	pixhist3d-pl.1 \
	pixhist3d.1 \
	pixinterp2x.1 \
	pixmatte.1 \
	pixmerge.1 \
	pixmorph.1 \
	pixrect.1 \
	pixrot.1 \
	pixsaturate.1 \
	pixscale.1 \
	pixshrink.1 \
	pixstat.1 \
	pixsubst.1 \
	pixtile.1 \
	pixuntile.1 \
	pl-X.1 \
	pl-X10.1 \
	pl-asc.1 \
	pl-hpgl.1 \
	pl-pl.1 \
	pl-ps.1 \
	pl-sgi.1 \
	pl-starbase.1 \
	pl-tek.1 \
	plcolor.1 \
	pldebug.1 \
	plgetframe.1 \
	plline2.1 \
	plrot.1 \
	png-bw.1 \
	png-pix.1 \
	png_info.1 \
	query.1 \
	rle-pix.1 \
	sun-pix.1 \
	terrain.1 \
	wavelet.1

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)

sample_applications_DATA = \
	pl-dm.c \
	roots_example.c

# XXX need to investigate why these .c are not being built
EXTRA_DIST = \
	$(man_MANS) \
	$(sample_applications_DATA) \
	fix_polysolids.c \
	lowp.c \
	pix-both-ik.c \
	pix-ci.c \
	pix-filt-ik.c \
	pix-ik.c \
	pix-ikr.c \
	pix-lowp-ik.c \
	pixt-ik.c \
	pl-X10.c \
	pl-starbase.c

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(alias_pix_OBJECTS) \
	$(ap_pix_OBJECTS) \
	$(asc_pl_OBJECTS) \
	$(azel_OBJECTS) \
	$(bary_OBJECTS) \
	$(binfo_OBJECTS) \
	$(buffer_OBJECTS) \
	$(bw_a_OBJECTS) \
	$(bw_imp_OBJECTS) \
	$(bw_pix_OBJECTS) \
	$(bw_png_OBJECTS) \
	$(bw_ps_OBJECTS) \
	$(bw_rle_OBJECTS) \
	$(bw3_pix_OBJECTS) \
	$(bwcrop_OBJECTS) \
	$(bwdiff_OBJECTS) \
	$(bwfilter_OBJECTS) \
	$(bwhist_OBJECTS) \
	$(bwhisteq_OBJECTS) \
	$(bwmod_OBJECTS) \
	$(bwrect_OBJECTS) \
	$(bwrot_OBJECTS) \
	$(bwscale_OBJECTS) \
	$(bwshrink_OBJECTS) \
	$(bwstat_OBJECTS) \
	$(bwthresh_OBJECTS) \
	$(cv_OBJECTS) \
	$(dbcp_OBJECTS) \
	$(decimate_OBJECTS) \
	$(double_asc_OBJECTS) \
	$(dpix_pix_OBJECTS) \
	$(dsp_add_OBJECTS) \
	$(dunncolor_OBJECTS) \
	$(dunnsnap_OBJECTS) \
	$(files_tape_OBJECTS) \
	$(gencolor_OBJECTS) \
	$(hd_OBJECTS) \
	$(imgdims_OBJECTS) \
	$(loop_OBJECTS) \
	$(mac_pix_OBJECTS) \
	$(msrandom_OBJECTS) \
	$(mst_OBJECTS) \
	$(op_bw_OBJECTS) \
	$(orle_pix_OBJECTS) \
	$(pcd_pix_OBJECTS) \
	$(pix_alias_OBJECTS) \
	$(pix_bw_OBJECTS) \
	$(pix_bw3_OBJECTS) \
	$(pix_orle_OBJECTS) \
	$(pix_png_OBJECTS) \
	$(pix_ppm_OBJECTS) \
	$(pix_ps_OBJECTS) \
	$(pix_rle_OBJECTS) \
	$(pix_spm_OBJECTS) \
	$(pix_sun_OBJECTS) \
	$(pix_yuv_OBJECTS) \
	$(pix3filter_OBJECTS) \
	$(pixbackgnd_OBJECTS) \
	$(pixbgstrip_OBJECTS) \
	$(pixblend_OBJECTS) \
	$(pixborder_OBJECTS) \
	$(pixbustup_OBJECTS) \
	$(pixclump_OBJECTS) \
	$(pixcolors_OBJECTS) \
	$(pixcount_OBJECTS) \
	$(pixcut_OBJECTS) \
	$(pixdiff_OBJECTS) \
	$(pixdsplit_OBJECTS) \
	$(pixelswap_OBJECTS) \
	$(pixembed_OBJECTS) \
	$(pixfade_OBJECTS) \
	$(pixfields_OBJECTS) \
	$(pixfieldsep_OBJECTS) \
	$(pixfilter_OBJECTS) \
	$(pixhalve_OBJECTS) \
	$(pixhist_OBJECTS) \
	$(pixhist3d_OBJECTS) \
	$(pixhist3d_pl_OBJECTS) \
	$(pixinterp2x_OBJECTS) \
	$(pixmatte_OBJECTS) \
	$(pixmerge_OBJECTS) \
	$(pixmorph_OBJECTS) \
	$(pixpaste_OBJECTS) \
	$(pixrect_OBJECTS) \
	$(pixrot_OBJECTS) \
	$(pixsaturate_OBJECTS) \
	$(pixscale_OBJECTS) \
	$(pixshrink_OBJECTS) \
	$(pixstat_OBJECTS) \
	$(pixsubst_OBJECTS) \
	$(pixtile_OBJECTS) \
	$(pixuntile_OBJECTS) \
	$(pl_X_OBJECTS) \
	$(pl_asc_OBJECTS) \
	$(pl_hpgl_OBJECTS) \
	$(pl_pl_OBJECTS) \
	$(pl_ps_OBJECTS) \
	$(pl_sgi_OBJECTS) \
	$(pl_tek_OBJECTS) \
	$(plcolor_OBJECTS) \
	$(pldebug_OBJECTS) \
	$(plgetframe_OBJECTS) \
	$(plline2_OBJECTS) \
	$(plrot_OBJECTS) \
	$(plstat_OBJECTS) \
	$(png_bw_OBJECTS) \
	$(png_pix_OBJECTS) \
	$(png_info_OBJECTS) \
	$(query_OBJECTS) \
	$(remapid_OBJECTS) \
	$(rle_pix_OBJECTS) \
	$(sgi_pix_OBJECTS) \
	$(sun_pix_OBJECTS) \
	$(terrain_OBJECTS) \
	$(texturescale_OBJECTS) \
	$(ttcp_OBJECTS) \
	$(wavelet_OBJECTS) \
	$(xyz_pl_OBJECTS) \
	$(yuv_pix_OBJECTS) \
	$(bin_PROGRAMS)
