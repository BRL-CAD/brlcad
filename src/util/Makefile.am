
if WITH_X11

PL_X = pl-X
PL_X_EXTRA =

PL_DM = pl-dm
PL_DM_EXTRA =

BOMBARDIER = bombardier
BOMBARDIER_EXTRA =

else !WITH_X11

PL_X =
PL_X_EXTRA = pl-X.c

PL_DM =
PL_DM_EXTRA = pl-dm.c

BOMBARDIER =
BOMBARDIER_EXTRA = bombardier.c

endif
# WITH_X11

if BUILD_BOOST
PC_TEST = pc_test
PC_TEST_EXTRA =
else
PC_TEST =
PC_TEST_EXTRA = pc_test.c
endif
# BUILD_BOOST


bin_PROGRAMS = \
	${BOMBARDIER} \
	${PL_DM} \
	${PL_X} \
	alias-pix \
	ap-pix \
	asc-pl \
	azel \
	bary \
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
	fix_polysolids \
	gencolor \
	hex \
	imgdims \
	loop \
	lowp \
	mac-pix \
	random \
	orle-pix \
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
	pl-asc \
	pl-hpgl \
	pl-pl \
	pl-ps \
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
	rle-pix \
	sun-pix \
	terrain \
	texturescale \
	ttcp \
	wavelet \
	xyz-pl \
	yuv-pix

AM_CPPFLAGS = ${TCL_CPPFLAGS} ${STRICT_FLAGS}

noinst_PROGRAMS = \
	${PC_TEST} \
	roots_example

dist_bin_SCRIPTS = \
	morphedit.tcl


##########################################
# All the sources and flags sorted below #
##########################################

alias_pix_SOURCES = alias-pix.c
alias_pix_LDADD = ${BU}

ap_pix_SOURCES = ap-pix.c
ap_pix_LDADD = ${BU}

asc_pl_SOURCES = asc-pl.c
asc_pl_LDADD = ${BN} ${BU}

azel_SOURCES = azel.c
azel_LDADD = ${BU}

bary_SOURCES = bary.c
bary_LDADD = ${BU}

if WITH_TK
bombardier_SOURCES = bombardier.c bombardier.h
bombardier_LDADD = ${TCLCAD} ${BU} ${TCL} ${TK}
bombardier_CFLAGS = ${TK_CPPFLAGS} ${TCL_CPPFLAGS}
endif

buffer_SOURCES = buffer.c
buffer_LDADD = ${BU}

bw3_pix_SOURCES = bw3-pix.c
bw3_pix_LDADD = ${BU}

bw_a_SOURCES = bw-a.c
bw_a_LDADD = ${FB} ${BU}

bw_imp_SOURCES = bw-imp.c
bw_imp_LDADD = ${BU}

bw_pix_SOURCES = bw-pix.c
bw_pix_LDADD = ${BU}

# per-program CPPFLAGS wasn't added until automake 1.7
bw_png_SOURCES = bw-png.c
bw_png_LDADD = ${FB} ${BU} ${PNG}
bw_png_CFLAGS = ${PNG_CPPFLAGS}

bw_ps_LDADD = ${BU}
bw_ps_SOURCES = bw-ps.c

# per-program CPPFLAGS wasn't added until automake 1.7
bw_rle_SOURCES = bw-rle.c
bw_rle_LDADD = ${RLE} ${BU} ${SYSV}
bw_rle_CFLAGS = ${RLE_CPPFLAGS}

bwcrop_SOURCES = bwcrop.c
bwcrop_LDADD = ${BU}

bwdiff_SOURCES = bwdiff.c
bwdiff_LDADD = ${BU}

bwfilter_SOURCES = bwfilter.c
bwfilter_LDADD = ${BU}

bwhist_SOURCES = bwhist.c
bwhist_LDADD = ${FB} ${BU}

bwhisteq_SOURCES = bwhisteq.c
bwhisteq_LDADD = ${BU}

bwmod_SOURCES = bwmod.c
bwmod_LDADD = ${BU}

bwrect_SOURCES = bwrect.c
bwrect_LDADD = ${BU}

bwrot_SOURCES = bwrot.c
bwrot_LDADD = ${BU}

bwscale_SOURCES = bwscale.c
bwscale_LDADD = ${BU}

bwshrink_SOURCES = bwshrink.c
bwshrink_LDADD = ${BU}

bwstat_SOURCES = bwstat.c
bwstat_LDADD = ${BU} ${LIBM}

bwthresh_SOURCES = bwthresh.c
bwthresh_LDADD = ${BU}

cv_SOURCES = cv.c
cv_LDADD = ${BU}

dbcp_SOURCES = dbcp.c
dbcp_LDADD = ${BU}

decimate_SOURCES = decimate.c
decimate_LDADD = ${BU}

double_asc_SOURCES = double-asc.c
double_asc_LDADD = ${FB} ${BU}

dpix_pix_SOURCES = dpix-pix.c
dpix_pix_LDADD = ${BU}

dsp_add_SOURCES = dsp_add.c
dsp_add_LDADD = ${BU}

dunncolor_SOURCES = dunncolor.c dunncomm.c
dunncolor_LDADD = ${BU}

dunnsnap_SOURCES = dunnsnap.c dunncomm.c
dunnsnap_LDADD = ${FB} ${BU}

fix_polysolids_SOURCES = fix_polysolids.c
fix_polysolids_LDADD = ${RT} ${BU}

gencolor_SOURCES = gencolor.c
gencolor_LDADD = ${BU}

hex_SOURCES = hex.c
hex_LDADD = ${BU}

imgdims_SOURCES = imgdims.c
imgdims_LDADD = ${FB} ${BU}

loop_SOURCES = loop.c
loop_LDADD = ${BU}

lowp_SOURCES = lowp.c
lowp_LDADD = ${BU}

mac_pix_SOURCES = mac-pix.c
mac_pix_LDADD = ${BU}

random_SOURCES = random.c
random_LDADD = ${BN} ${BU}

orle_pix_SOURCES = orle-pix.c
orle_pix_LDADD = ${FB} ${BU} ${ORLE}

pix3filter_SOURCES = pix3filter.c
pix3filter_LDADD = ${BU}

pix_alias_SOURCES = pix-alias.c
pix_alias_LDADD = ${BU}

pix_bw3_SOURCES = pix-bw3.c
pix_bw3_LDADD = ${BU}

pix_bw_SOURCES = pix-bw.c
pix_bw_LDADD = ${BU}

pix_orle_SOURCES = pix-orle.c
pix_orle_LDADD = ${FB} ${BU} ${ORLE}

# per-program CPPFLAGS wasn't added until automake 1.7
pix_png_SOURCES = pix-png.c
pix_png_CFLAGS = ${PNG_CPPFLAGS}
pix_png_LDADD = ${FB} ${BU}

pix_ppm_SOURCES = pix-ppm.c
pix_ppm_LDADD = ${FB} ${BU}

pix_ps_SOURCES = pix-ps.c
pix_ps_LDADD = ${BU}

# per-program CPPFLAGS wasn't added until automake 1.7
pix_rle_SOURCES = pix-rle.c
pix_rle_LDADD = ${RLE} ${BU} ${SYSV}
pix_rle_CFLAGS = ${RLE_CPPFLAGS}

pix_spm_SOURCES = pix-spm.c
pix_spm_LDADD = ${BN}

pix_sun_SOURCES = pix-sun.c
pix_sun_LDADD = ${BU}

pix_yuv_SOURCES = pix-yuv.c
pix_yuv_LDADD = ${FB} ${BN} ${BU}

pixbackgnd_SOURCES = pixbackgnd.c
pixbackgnd_LDADD = ${BU}

pixbgstrip_SOURCES = pixbgstrip.c
pixbgstrip_LDADD = ${FB} ${BU}

pixblend_SOURCES = pixblend.c
pixblend_LDADD = ${BU}

pixborder_SOURCES = pixborder.c
pixborder_LDADD = ${FB} ${BN} ${BU}

pixbustup_SOURCES = pixbustup.c
pixbustup_LDADD = ${BU}

pixclump_SOURCES = pixclump.c
pixclump_LDADD = ${BN} ${BU}

pixcolors_SOURCES = pixcolors.c
pixcolors_LDADD = ${BU}

pixcount_SOURCES = pixcount.c
pixcount_LDADD = ${BU}

pixcut_SOURCES = pixcut.c
pixcut_LDADD = ${FB} ${BU}

pixdiff_SOURCES = pixdiff.c
pixdiff_LDADD = ${BU}

pixdsplit_SOURCES = pixdsplit.c
pixdsplit_LDADD = ${BU}

pixelswap_SOURCES = pixelswap.c
pixelswap_LDADD = ${BU}

pixembed_SOURCES = pixembed.c
pixembed_LDADD = ${BU}

pixfade_SOURCES = pixfade.c
pixfade_LDADD = ${BN} ${BU}

pixfields_SOURCES = pixfields.c
pixfields_LDADD = ${BU}

pixfieldsep_SOURCES = pixfieldsep.c
pixfieldsep_LDADD = ${BU}

pixfilter_SOURCES = pixfilter.c
pixfilter_LDADD = ${BU}

pixhalve_SOURCES = pixhalve.c
pixhalve_LDADD = ${FB} ${BU}

pixhist3d_SOURCES = pixhist3d.c
pixhist3d_LDADD = ${FB} ${BU}

pixhist3d_pl_SOURCES = pixhist3d-pl.c
pixhist3d_pl_LDADD = ${BN} ${BU}

pixhist_SOURCES = pixhist.c
pixhist_LDADD = ${FB} ${BU}

pixinterp2x_SOURCES = pixinterp2x.c
pixinterp2x_LDADD = ${BU}

pixmatte_SOURCES = pixmatte.c
pixmatte_LDADD = ${BU}

pixmerge_SOURCES = pixmerge.c
pixmerge_LDADD = ${BU}

pixmorph_SOURCES = pixmorph.c
pixmorph_LDADD = ${FB} ${BU}

pixpaste_SOURCES = pixpaste.c
pixpaste_LDADD = ${FB} ${BU}

pixrect_SOURCES = pixrect.c
pixrect_LDADD = ${BU}

pixrot_SOURCES = pixrot.c
pixrot_LDADD = ${BU}

pixsaturate_SOURCES = pixsaturate.c
pixsaturate_LDADD = ${BU}

pixscale_SOURCES = pixscale.c
pixscale_LDADD = ${BU}

pixshrink_SOURCES = pixshrink.c
pixshrink_LDADD = ${BU}

pixstat_SOURCES = pixstat.c
pixstat_LDADD = ${BU}

pixsubst_SOURCES = pixsubst.c
pixsubst_LDADD = ${BU}

pixtile_SOURCES = pixtile.c
pixtile_LDADD = ${BU}

pixuntile_SOURCES = pixuntile.c
pixuntile_LDADD = ${BU}

if WITH_X11
pl_X_SOURCES = pl-X.c
pl_X_LDADD = ${DM} ${DM_LIBS} ${BU}
pl_X_CFLAGS = ${X_CFLAGS}

pl_dm_SOURCES = pl-dm.c
pl_dm_LDADD = ${DM} ${DM_LIBS} ${BU} ${TK}
pl_dm_CFLAGS = ${TK_CPPFLAGS} ${X_CFLAGS}
endif
# WITH_X11

pl_asc_SOURCES = pl-asc.c
pl_asc_LDADD = ${BU}

pl_hpgl_SOURCES = pl-hpgl.c
pl_hpgl_LDADD = ${BU}

pl_pl_SOURCES = pl-pl.c
pl_pl_LDADD = ${BU}

pl_ps_SOURCES = pl-ps.c
pl_ps_LDADD = ${BU}

pl_tek_SOURCES = pl-tek.c
pl_tek_LDADD = ${BU}

plcolor_SOURCES = plcolor.c
plcolor_LDADD = ${BN}

pldebug_SOURCES = pldebug.c
pldebug_LDADD = ${BU}

plgetframe_SOURCES = plgetframe.c
plgetframe_LDADD = ${BU}

plline2_SOURCES = plline2.c
plline2_LDADD = ${BN} ${BU}

plrot_SOURCES = plrot.c
plrot_LDADD = ${BN} ${BU}

plstat_SOURCES = plstat.c
plstat_LDADD = ${BU}

# per-program CPPFLAGS wasn't added until automake 1.7
png_bw_SOURCES = png-bw.c
png_bw_CFLAGS = ${PNG_CPPFLAGS}
png_bw_LDADD = ${BN} ${BU}

# per-program CPPFLAGS wasn't added until automake 1.7
png_info_SOURCES = png_info.c
png_info_CFLAGS = ${PNG_CPPFLAGS}
png_info_LDADD = ${BN} ${BU}

# per-program CPPFLAGS wasn't added until automake 1.7
png_pix_SOURCES = png-pix.c
png_pix_CFLAGS = ${PNG_CPPFLAGS}
png_pix_LDADD = ${BN} ${BU}

rle_pix_SOURCES = rle-pix.c
rle_pix_LDADD = ${RLE} ${BU} ${SYSV}
rle_pix_CFLAGS = ${RLE_CPPFLAGS}

roots_example_SOURCES = roots_example.c
roots_example_LDADD = ${RT} ${BU}

if BUILD_BOOST
pc_test_SOURCES = pc_test.c
pc_test_LDADD = ${PC} ${PC_LIBS} ${WDB} ${WDB_LIBS}
endif
# BUILD_BOOST

sun_pix_SOURCES = sun-pix.c
sun_pix_LDADD = ${BU}

terrain_SOURCES = terrain.c
terrain_LDADD = ${BN} ${BU}

texturescale_SOURCES = texturescale.c
texturescale_LDADD = ${FB} ${BN} ${BU}

ttcp_SOURCES = ttcp.c
ttcp_LDADD = ${LIBSOCKET} ${LIBNSL} ${LIBNETWORK}

wavelet_SOURCES = wavelet.c
wavelet_LDADD = ${FB} ${BN} ${BU}

xyz_pl_SOURCES = xyz-pl.c
xyz_pl_LDADD = ${BN} ${BU}

yuv_pix_SOURCES = yuv-pix.c
yuv_pix_LDADD = ${FB} ${BN} ${BU}

man_MANS = \
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
	hex.1 \
	imgdims.1 \
	loop.1 \
	mac-pix.1 \
	morphedit.1 \
	orle-pix.1 \
	pix-alias.1 \
	pix-bw.1 \
	pix-bw3.1 \
	pix-orle.1 \
	pix-png.1 \
	pix-ppm.1 \
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
	pl-asc.1 \
	pl-hpgl.1 \
	pl-pl.1 \
	pl-ps.1 \
	pl-tek.1 \
	plcolor.1 \
	pldebug.1 \
	plgetframe.1 \
	plline2.1 \
	plrot.1 \
	png-bw.1 \
	png-pix.1 \
	png_info.1 \
	random.1 \
	rle-pix.1 \
	sun-pix.1 \
	terrain.1 \
	wavelet.1

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)

sample_applications_DATA = \
	pl-dm.c \
	roots_example.c

EXTRA_DIST = \
	$(man_MANS) \
	$(sample_applications_DATA) \
	${BOMBARDIER_EXTRA} \
	${PC_TEST_EXTRA} \
	${PL_DM_EXTRA} \
	${PL_X_EXTRA} \
	CMakeLists.txt

DEPENDS = src/liborle src/libdm src/libfb src/libpc

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(alias_pix_OBJECTS) \
	$(ap_pix_OBJECTS) \
	$(asc_pl_OBJECTS) \
	$(azel_OBJECTS) \
	$(bary_OBJECTS) \
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
	$(gencolor_OBJECTS) \
	$(hex_OBJECTS) \
	$(imgdims_OBJECTS) \
	$(loop_OBJECTS) \
	$(mac_pix_OBJECTS) \
	$(op_bw_OBJECTS) \
	$(orle_pix_OBJECTS) \
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
	$(random_OBJECTS) \
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
