# $Id$

if ONLY_BENCHMARK

bin_PROGRAMS = \
	asc2g \
	asc2pix \
	g2asc \
	pix2asc

EXTRA_DIST = 
	dbclean.sh \
	g-xxx.c \
	g-xxx_facets.c \
	g4-g5.c \
	g5-g4.c

else !ONLY_BENCHMARK

bin_PROGRAMS = \
	asc-nmg \
	asc2g \
	asc2pix \
	bot-bldxf \
	bot-raw \
	bot_shell-vtk \
	conv-vg2g \
	cy-g \
	dbupgrade \
	dxf-g \
	enf-g \
	euclid-g \
	euclid_format \
	euclid_unformat \
	fast4-g \
	g-acad \
	g-dxf \
	g-euclid \
	g-euclid1 \
	g-nff \
	g-nmg \
	g-shell.rect \
	g-stl \
	g-tankill \
	g-vrml \
	g-wave \
	g-x3d \
	g2asc \
	nastran-g \
	nmg-bot \
	nmg-rib \
	nmg-sgp \
	pix2asc \
	ply-g \
	poly-bot \
	proe-g \
	stl-g \
	tankill-g \
	viewpoint-g

noinst_PROGRAMS = \
	g4-g5 \
	g5-g4 \
	g-xxx \
	g-xxx_facets

EXTRA_DIST = dbclean.sh

endif
# end ONLY_BENCHMARK

asc2g_LDADD = ${WDB} ${LIBRT} ${TCL}
asc_nmg_LDADD = ${WDB} ${LIBRT} 
bot_bldxf_LDADD = ${LIBRT}
bot_raw_LDADD = ${LIBRT}
bot_shell_vtk_LDADD = ${LIBRT} ${SYSV} 
cy_g_LDADD = ${WDB} ${LIBRT} 
dbupgrade_LDADD = ${WDB} ${LIBRT} 
dxf_g_LDADD = ${WDB} ${LIBRT} 
enf_g_LDADD = ${WDB} ${LIBRT} ${SYSV} ${TCL}
euclid_format_LDADD = ${LIBRT} 
euclid_g_LDADD = ${WDB} ${LIBRT} 
fast4_g_LDADD = ${WDB} ${LIBRT} 
g2asc_LDADD = ${WDB} ${LIBRT} ${TCL}
g4_g5_LDADD = ${WDB} ${LIBRT}
g5_g4_LDADD = ${WDB} ${LIBRT}
g_acad_LDADD = ${WDB} ${LIBRT} 
g_dxf_LDADD = ${WDB} ${LIBRT} 
g_euclid1_LDADD = ${LIBRT} 
g_euclid_LDADD = ${LIBRT} 
g_nff_LDADD = ${WDB} ${LIBRT} 
g_nmg_LDADD = ${WDB} ${LIBRT} 
g_shell_rect_LDADD = ${WDB} ${LIBRT} 
g_stl_LDADD = ${WDB} ${LIBRT} 
g_tankill_LDADD = ${LIBRT} 
g_vrml_LDADD = ${WDB} ${LIBRT} 
g_wave_LDADD = ${WDB} ${LIBRT} 
g_x3d_LDADD = ${WDB} ${LIBRT} 
g_xxx_LDADD = ${WDB} ${LIBRT} 
g_xxx_facets_LDADD = ${WDB} ${LIBRT} 
nastran_g_LDADD = ${WDB} ${LIBRT} 
nmg_bot_LDADD = ${WDB} ${LIBRT} 
nmg_rib_LDADD = ${WDB} ${LIBRT} 
nmg_sgp_LDADD = ${WDB} ${LIBRT} 
ply_g_LDADD = ${WDB} ${LIBRT} ${SYSV} 
poly_bot_LDADD = ${WDB} ${LIBRT} 
proe_g_LDADD = ${WDB} ${LIBRT} ${SYSV} ${TCL}
stl_g_LDADD = ${WDB} ${LIBRT} ${SYSV} 
tankill_g_LDADD = ${WDB} ${LIBRT} 
viewpoint_g_LDADD = ${WDB} ${LIBRT} 

asc2g_SOURCES = asc2g.c 
asc2pix_SOURCES = asc2pix.c
asc_nmg_SOURCES = asc-nmg.c 
bot_bldxf_SOURCES = bot-bldxf.c
bot_raw_SOURCES = bot-raw.c
bot_shell_vtk_SOURCES = bot_shell-vtk.c 
conv_vg2g_SOURCES = conv-vg2g.c
cy_g_SOURCES = cy-g.c 
dbupgrade_SOURCES = dbupgrade.c 
dxf_g_SOURCES = dxf-g.c 
enf_g_SOURCES = enf-g.c 
euclid_format_SOURCES = euclid_format.c 
euclid_g_SOURCES = euclid-g.c 
euclid_unformat_SOURCES = euclid_unformat.c
fast4_g_SOURCES = fast4-g.c 
g2asc_SOURCES = g2asc.c 
g4_g5_SOURCES = g4-g5.c
g5_g4_SOURCES = g5-g4.c
g_acad_SOURCES = g-acad.c 
g_dxf_SOURCES = g-dxf.c 
g_euclid1_SOURCES = g-euclid1.c 
g_euclid_SOURCES = g-euclid.c 
g_nff_SOURCES = g-nff.c 
g_nmg_SOURCES = g-nmg.c 
g_shell_rect_SOURCES = g-shell.rect.c 
g_stl_SOURCES = g-stl.c 
g_tankill_SOURCES = g-tankill.c 
g_vrml_SOURCES = g-vrml.c 
g_wave_SOURCES = g-wave.c 
g_x3d_SOURCES = g-x3d.c 
g_xxx_SOURCES = g-xxx.c 
g_xxx_facets_SOURCES = g-xxx_facets.c 
nastran_g_SOURCES = nastran-g.c 
nmg_bot_SOURCES = nmg-bot.c 
nmg_rib_SOURCES = nmg-rib.c 
nmg_sgp_SOURCES = nmg-sgp.c 
pix2asc_SOURCES = pix2asc.c
ply_g_SOURCES = ply-g.c 
poly_bot_SOURCES = poly-bot.c 
proe_g_SOURCES = proe-g.c 
stl_g_SOURCES = stl-g.c 
tankill_g_SOURCES = tankill-g.c 
viewpoint_g_SOURCES = viewpoint-g.c 

dist_man_MANS = \
	asc-nmg.1 \
	bot_shell-vtk.1 \
	conv-vg2g.1 \
	cy-g.1 \
	dbclean.1 \
	dxf-g.1 \
	enf-g.1 \
	euclid-g.1 \
	fast4-g.1 \
	g-acad.1 \
	g-dxf.1 \
	g-euclid.1 \
	g-nff.1 \
	g-nmg.1 \
	g-shell.rect.1 \
	g-stl.1 \
	g-tankill.1 \
	g-vrml.1 \
	g-wave.1 \
	g-x3d.1 \
	g2asc.1 \
	nastran-g.1 \
	nmg-bot.1 \
	nmg-rib.1 \
	pix2asc.1 \
	ply-g.1 \
	poly-bot.1 \
	proe-g.1 \
	stl-g.1 \
	tankill-g.1 \
	viewpoint-g.1

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)

sample_applications_DATA = \
	g-xxx.c \
	g-xxx_facets.c

MAINTAINERCLEANFILES = Makefile.in
