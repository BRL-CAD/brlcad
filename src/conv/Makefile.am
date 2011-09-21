if BUILD_STEP
STEPDIR=step
else !BUILD_STEP
STEPDIR=
endif

IGESDIR=iges
INTAVALDIR=intaval

EXTRA_DIST = \
	CMakeLists.txt \
	Formats.csv \
	comgeom/try.sh \
	dbclean.sh \
	g-xxx.c \
	g-xxx_facets.c \
	g4-g5.c \
	g5-g4.c \
	obj-g_new.c \
	patch/pull_comp.sh \
	patch/pull_solidsub.sh \
	patch/rpatch.f \
	shp/README \
	shp/shapelib/LICENSE.LGPL \
	shp/shapelib/license.html \
	walk_example.c

if ONLY_BENCHMARK

SUBDIRS = .

bin_PROGRAMS = \
	asc2g \
	asc2pix \
	g2asc \
	pix2asc

else !ONLY_BENCHMARK

SUBDIRS = \
	${IGESDIR} \
	${INTAVALDIR}
	${STEPDIR}

DIST_SUBDIRS = \
	iges \
	intaval \
	step

bin_PROGRAMS = \
	3dm-g \
	asc-nmg \
	asc2g \
	asc2pix \
	bot-bldxf \
	bot_dump \
	bot_shell-vtk \
	comgeom-g \
	conv-vg2g \
	cy-g \
	dbupgrade \
	dem-g \
	dxf-g \
	enf-g \
	euclid-g \
	euclid_format \
	euclid_unformat \
	fast4-g \
	g-acad \
	g-dot \
	g-dxf \
	g-egg \
	g-euclid \
	g-euclid1 \
	g-jack \
	g-nff \
	g-nmg \
	g-obj \
	g-off \
	g-shell-rect \
	g-stl \
	g-tankill \
	g-var \
	g-vrml \
	g-x3d \
	g2asc \
	jack-g \
	nastran-g \
	nmg-bot \
	nmg-rib \
	nmg-sgp \
	obj-g \
	off-g \
	patch-g \
	pix2asc \
	ply-g \
	poly-bot \
	proe-g \
	rpatch \
	shp-g \
	stl-g \
	tankill-g \
	viewpoint-g

noinst_PROGRAMS = \
	g4-g5 \
	g5-g4 \
	g-xxx \
	g-xxx_facets \
	walk_example

endif
# end ONLY_BENCHMARK


3dm_g_SOURCES = 3dm/3dm-g.cpp
if BUILD_OPENNURBS
3dm_g_CXXFLAGS = -DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS} ${TCL_CPPFLAGS}
else
3dm_g_CXXFLAGS =
endif
3dm_g_LDADD = \
	${WDB} \
	${WDB_LIBS} \
	${OPENNURBS}

asc2g_SOURCES = asc/asc2g.c
asc2g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
asc2g_LDADD = ${WDB} ${TCLCAD} ${GED}

asc2pix_SOURCES = asc/asc2pix.c
asc2pix_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}

asc_nmg_SOURCES = nmg/asc-nmg.c
asc_nmg_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
asc_nmg_LDADD = ${WDB}

bot_dump_SOURCES = bot_dump.c
bot_dump_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
bot_dump_LDADD = ${RT} ${GED}

bot_bldxf_SOURCES = dxf/bot-bldxf.c
bot_bldxf_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
bot_bldxf_LDADD = ${RT}

bot_shell_vtk_SOURCES = bot_shell-vtk.c
bot_shell_vtk_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
bot_shell_vtk_LDADD = ${RT}

comgeom_g_SOURCES = \
	comgeom/cvt.c \
	comgeom/f2a.c \
	comgeom/mat.c \
	comgeom/read.c \
	comgeom/region.c \
	comgeom/solid.c \
	comgeom/tools.c
comgeom_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
comgeom_g_LDADD = \
	${WDB} \
	${BU} \
	${BU_LIBS}

conv_vg2g_SOURCES = conv-vg2g.c
conv_vg2g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
conv_vg2g_LDADD = ${BU} ${SYSV}

cy_g_SOURCES = cy-g.c
cy_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
cy_g_LDADD = ${WDB}

dbupgrade_SOURCES = dbupgrade.c
dbupgrade_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
dbupgrade_LDADD = ${WDB}

dem_g_SOURCES = dem-g.c
dem_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
dem_g_LDADD = ${WDB}

dxf_g_SOURCES = dxf/dxf-g.c
dxf_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
dxf_g_LDADD = ${WDB}

enf_g_SOURCES = enf-g.c
enf_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
enf_g_LDADD = ${WDB}

euclid_format_SOURCES = euclid/euclid_format.c
euclid_format_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
euclid_format_LDADD = ${RT}

euclid_g_SOURCES = euclid/euclid-g.c
euclid_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
euclid_g_LDADD = ${WDB}

euclid_unformat_SOURCES = euclid/euclid_unformat.c
euclid_unformat_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
euclid_unformat_LDADD = ${BU} ${BN} ${SYSV}

fast4_g_SOURCES = fast4-g.c
fast4_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
fast4_g_LDADD = ${WDB}

g2asc_SOURCES = asc/g2asc.c
g2asc_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g2asc_LDADD = ${WDB}

g4_g5_SOURCES = g4-g5.c
g4_g5_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g4_g5_LDADD = ${WDB}

g5_g4_SOURCES = g5-g4.c
g5_g4_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g5_g4_LDADD = ${WDB}

g_acad_SOURCES = g-acad.c
g_acad_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_acad_LDADD = ${WDB}

g_dot_SOURCES = g-dot.c
g_dot_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_dot_LDADD = ${GED}

g_dxf_SOURCES = dxf/g-dxf.c
g_dxf_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_dxf_LDADD = ${WDB} ${GCV}

g_egg_SOURCES = g-egg.c
g_egg_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_egg_LDADD = ${WDB} ${GCV}

g_euclid1_SOURCES = euclid/g-euclid1.c
g_euclid1_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_euclid1_LDADD = ${RT}

g_euclid_SOURCES = euclid/g-euclid.c
g_euclid_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_euclid_LDADD = ${RT}

g_jack_SOURCES = jack/g-jack.c
g_jack_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_jack_LDADD = \
	${WDB} \
	${WDB_LIBS}

g_nff_SOURCES = g-nff.c
g_nff_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_nff_LDADD = ${WDB}

g_nmg_SOURCES = nmg/g-nmg.c
g_nmg_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_nmg_LDADD = ${WDB}

g_obj_SOURCES = g-obj.c
g_obj_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_obj_LDADD = ${WDB}

g_off_SOURCES = off/g-off.c
g_off_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_off_LDADD = \
	${RT} \
	${RT_LIBS}

g_shell_rect_SOURCES = g-shell-rect.c
g_shell_rect_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_shell_rect_LDADD = ${WDB}

g_stl_SOURCES = stl/g-stl.c
g_stl_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_stl_LDADD = ${WDB} ${GCV}

g_tankill_SOURCES = tankill/g-tankill.c
g_tankill_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_tankill_LDADD = ${RT}

g_var_SOURCES = g-var.c
g_var_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_var_LDADD = ${RT}

g_vrml_SOURCES = g-vrml.c
g_vrml_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_vrml_LDADD = ${WDB}

g_x3d_SOURCES = g-x3d.c
g_x3d_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_x3d_LDADD = ${WDB}

g_xxx_SOURCES = g-xxx.c
g_xxx_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_xxx_LDADD = ${WDB}

g_xxx_facets_SOURCES = g-xxx_facets.c
g_xxx_facets_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
g_xxx_facets_LDADD = ${WDB}

jack_g_SOURCES = jack/jack-g.c
jack_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
jack_g_LDADD = \
	${WDB} \
	${WDB_LIBS}

nastran_g_SOURCES = nastran-g.c
nastran_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
nastran_g_LDADD = ${WDB}

nmg_bot_SOURCES = nmg/nmg-bot.c
nmg_bot_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
nmg_bot_LDADD = ${WDB}

nmg_rib_SOURCES = nmg/nmg-rib.c
nmg_rib_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
nmg_rib_LDADD = ${WDB}

nmg_sgp_SOURCES = nmg/nmg-sgp.c
nmg_sgp_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
nmg_sgp_LDADD = ${WDB}

off_g_SOURCES = off/off-g.c
off_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
off_g_LDADD = \
	${WDB} \
	${WDB_LIBS}

obj_g_SOURCES = obj-g.c
obj_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
obj_g_LDADD = ${WDB}

patch_g_SOURCES = patch/patch-g.c
patch_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
patch_g_LDADD = \
	${WDB}\
	${WDB_LIBS}

pix2asc_SOURCES = asc/pix2asc.c
pix2asc_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}

ply_g_SOURCES = ply-g.c
ply_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
ply_g_LDADD = ${WDB}

poly_bot_SOURCES = poly-bot.c
poly_bot_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
poly_bot_LDADD = ${WDB}

proe_g_SOURCES = proe-g.c
proe_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
proe_g_LDADD = ${WDB}

rpatch_SOURCES = patch/rpatch.c
rpatch_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
rpatch_LDADD = \
	${BU} \
	${BU_LIBS} \
	${SYSV}

shp_g_SOURCES = shp/shp-g.c shp/shapelib/safileio.c shp/shapelib/shpopen.c
shp_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
shp_g_LDADD = ${WDB}

stl_g_SOURCES = stl/stl-g.c
stl_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
stl_g_LDADD = ${WDB}

tankill_g_SOURCES = tankill/tankill-g.c
tankill_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
tankill_g_LDADD = ${WDB}

viewpoint_g_SOURCES = viewpoint-g.c
viewpoint_g_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
viewpoint_g_LDADD = ${WDB}

walk_example_SOURCES = walk_example.c
walk_example_CFLAGS = ${STRICT_FLAGS} ${TCL_CPPFLAGS}
walk_example_LDADD = ${RT}

dist_bin_SCRIPTS = intaval-g.py

noinst_HEADERS = \
	comgeom/3d.h \
	dxf/dxf.h \
	patch/patch-g.h \
	shp/shapelib/shapefil.h

dist_man_MANS = \
	asc/g2asc.1 \
	asc/pix2asc.1 \
	bot_dump.1 \
	bot_shell-vtk.1 \
	comgeom/comgeom-g.1 \
	conv-vg2g.1 \
	cy-g.1 \
	dbclean.1 \
	dbupgrade.1 \
	dxf/dxf-g.1 \
	dxf/g-dxf.1 \
	enf-g.1 \
	euclid/euclid-g.1 \
	euclid/g-euclid.1 \
	fast4-g.1 \
	g-acad.1 \
	g-egg.1 \
	g-nff.1 \
	g-obj.1 \
	g-shell-rect.1 \
	g-var.1 \
	g-vrml.1 \
	g-x3d.1 \
	nastran-g.1 \
	nmg/asc-nmg.1 \
	nmg/g-nmg.1 \
	nmg/nmg-bot.1 \
	nmg/nmg-rib.1 \
	patch/patch-g.1 \
	patch/rpatch.1 \
	ply-g.1 \
	poly-bot.1 \
	proe-g.1 \
	stl/g-stl.1 \
	stl/stl-g.1 \
	tankill/g-tankill.1 \
	tankill/tankill-g.1 \
	viewpoint-g.1

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)

sample_applications_DATA = \
	g-xxx.c \
	g-xxx_facets.c

DEPENDS = src/libged src/libwdb src/libgcv

include $(top_srcdir)/misc/Makefile.defs

if ONLY_BENCHMARK
FAST_OBJECTS = \
	$(asc2g_OBJECTS) \
	$(asc2pix_OBJECTS) \
	$(g2asc_OBJECTS) \
	$(pix2asc_OBJECTS) \
	$(bin_PROGRAMS)

else !ONLY_BENCHMARK

FAST_OBJECTS = \
	$(3dm_g_OBJECTS) \
	$(asc2g_OBJECTS) \
	$(asc2pix_OBJECTS) \
	$(asc_nmg_OBJECTS) \
	$(bot_bldxf_OBJECTS) \
	$(bot_dump_OBJECTS) \
	$(bot_raw_OBJECTS) \
	$(bot_shell_vtk_OBJECTS) \
	$(comgeom_g_OBJECTS) \
	$(conv_vg2g_OBJECTS) \
	$(cy_g_OBJECTS) \
	$(dbupgrade_OBJECTS) \
	$(dem_g_OBJECTS) \
	$(dxf_g_OBJECTS) \
	$(enf_g_OBJECTS) \
	$(euclid_format_OBJECTS) \
	$(euclid_g_OBJECTS) \
	$(euclid_unformat_OBJECTS) \
	$(fast4_g_OBJECTS) \
	$(g2asc_OBJECTS) \
	$(g4_g5_OBJECTS) \
	$(g5_g4_OBJECTS) \
	$(g_acad_OBJECTS) \
	$(g_adrt_OBJECTS) \
	$(g_dxf_OBJECTS) \
	$(g_egg) \
	$(g_euclid1_OBJECTS) \
	$(g_euclid_OBJECTS) \
	$(g_jack_OBJECTS) \
	$(g_nff_OBJECTS) \
	$(g_nmg_OBJECTS) \
	$(g_obj_OBJECTS) \
	$(g_off_OBJECTS) \
	$(g_shell_rect_OBJECTS) \
	$(g_stl_OBJECTS) \
	$(g_tankill_OBJECTS) \
	$(g_var_OBJECTS) \
	$(g_vrml_OBJECTS) \
	$(g_x3d_OBJECTS) \
	$(g_xxx_OBJECTS) \
	$(g_xxx_facets_OBJECTS) \
	$(jack_g_OBJECTS) \
	$(nastran_g_OBJECTS) \
	$(nmg_bot_OBJECTS) \
	$(nmg_rib_OBJECTS) \
	$(nmg_sgp_OBJECTS) \
	$(off_g_OBJECTS) \
	$(patch_g_OBJECTS) \
	$(pix2asc_OBJECTS) \
	$(ply_g_OBJECTS) \
	$(poly_bot_OBJECTS) \
	$(proe_g_OBJECTS) \
	$(rpatch_OBJECTS) \
	$(stl_g_OBJECTS) \
	$(tankill_g_OBJECTS) \
	$(viewpoint_g_OBJECTS) \
	$(bin_PROGRAMS)

endif
# end ONLY_BENCHMARK
