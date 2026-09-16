/*                         G - O B J . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file conv/g-obj.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a Wavefront
 * '.obj' file by calling on the NMG booleans.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/opt.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4

extern union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);

static char usage[] =
    "[-m][-v][-i][-u][-xX lvl][-a abs_tess_tol][-r rel_tess_tol][-n norm_tess_tol][-P #_of_CPUs]\n"
    "[-e error_file_name ][-D dist_calc_tol][-o output_file_name ] brlcad_db.g object(s)\n";

static void
print_usage(const char *progname)
{
    bu_exit(1, "Usage: %s %s", progname, usage);
}


static b_off_t vert_offset=0;
static b_off_t norm_offset=0;
static int do_normals=0;
static int NMG_debug;	/* saved arg of -X, for longjmp handling */
static int verbose=0;
static int usemtl=0;	/* flag to include 'usemtl' statements with a
			 * code for GIFT materials:
			 *
			 * usemtl 0_100_32
			 * means aircode is 0
			 * los is 100
			 * GIFT material is 32
			 */
static int ncpu = 1;	/* Number of processors */
static char *output_file = NULL;	/* output filename */
static char *error_file = NULL;		/* error filename */
static FILE *fp;			/* Output file pointer */
static FILE *fpe;			/* Error file pointer */
static FILE *mtl_fp = NULL;		/* Companion material file pointer */
static struct db_i *dbip;
static struct bg_tess_tol ttol;
static struct bn_tol tol;
static struct model *the_model;
static struct bu_list material_head;
static struct bu_vls material_lib_name = BU_VLS_INIT_ZERO;

static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;
static int inches = 0;
static int print_help = 0;
static int warned_missing_mtl = 0;


struct obj_material_entry {
    struct bu_list l;
    char *name;
};


static void
obj_rgb_from_mater(unsigned char rgb[3], const struct mater_info *mater)
{
    size_t i;

    for (i = 0; i < 3; ++i) {
	int channel = (int)lrint(mater->ma_color[i] * 255.0);

	if (channel < 0)
	    channel = 0;
	if (channel > 255)
	    channel = 255;
	rgb[i] = (unsigned char)channel;
    }
}


static int
obj_get_path_color(struct db_i *idbip, const struct db_full_path *pathp, unsigned char rgb[3])
{
    size_t i;
    int found = 0;

    if (!idbip || !pathp)
	return 0;

    for (i = 0; i < pathp->fp_len; ++i) {
	struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	const char *color_val;
	int inherit;

	if (db5_get_attributes(idbip, &avs, pathp->fp_names[i]) != 0)
	    continue;

	inherit = BU_STR_EQUAL(bu_avs_get(&avs, "inherit"), "1");

	if (db_mater_head(idbip)) {
	    int region_id = -1;
	    const char *region_id_val = bu_avs_get(&avs, "region_id");
	    const struct mater *mp;

	    if (region_id_val) {
		bu_opt_int(NULL, 1, &region_id_val, (void *)&region_id);
	    } else if (pathp->fp_names[i]->d_flags & RT_DIR_REGION) {
		region_id = 0;
	    }

	    if (region_id >= 0) {
		for (mp = db_mater_head(idbip); mp != MATER_NULL; mp = mp->mt_forw) {
		    if (region_id > mp->mt_high || region_id < mp->mt_low)
			continue;
		    rgb[0] = mp->mt_r;
		    rgb[1] = mp->mt_g;
		    rgb[2] = mp->mt_b;
		    found = 1;
		    break;
		}
		if (found && inherit) {
		    bu_avs_free(&avs);
		    return 1;
		}
		if (found) {
		    bu_avs_free(&avs);
		    continue;
		}
	    }
	}

	color_val = bu_avs_get(&avs, "color");
	if (!color_val)
	    color_val = bu_avs_get(&avs, "rgb");
	if (color_val && bu_str_to_rgb(color_val, rgb))
	    found = 1;

	bu_avs_free(&avs);

	if (found && inherit)
	    return 1;
    }

    return found;
}


static int
obj_get_export_color(struct db_i *idbip, int region_id, const struct mater_info *mater, unsigned char rgb[3])
{
    struct region reg;

    if (mater->ma_color_valid) {
	obj_rgb_from_mater(rgb, mater);
	return 1;
    }

    if (!idbip)
	return 0;

    memset(&reg, 0, sizeof(reg));
    reg.reg_regionid = region_id;
    reg.reg_mater = *mater;
    db_mater_color_region(idbip, &reg);

    if (!reg.reg_mater.ma_color_valid)
	return 0;

    obj_rgb_from_mater(rgb, &reg.reg_mater);
    return 1;
}


static int
obj_material_name(struct bu_vls *name, int color_valid, const unsigned char rgb[3], int preserve_gift, int aircode, int los, int material_id)
{
    bu_vls_trunc(name, 0);

    if (color_valid) {
	if (preserve_gift) {
	    bu_vls_sprintf(name, "mat_%d_%d_%d__color_%u_%u_%u",
			   material_id, los, aircode, rgb[0], rgb[1], rgb[2]);
	} else {
	    bu_vls_sprintf(name, "color_%u_%u_%u", rgb[0], rgb[1], rgb[2]);
	}
	return 1;
    }

    if (preserve_gift) {
	bu_vls_sprintf(name, "mat_%d_%d_%d", material_id, los, aircode);
	return 1;
    }

    return 0;
}


static int
obj_material_exists(const char *name)
{
    struct obj_material_entry *entry;

    for (BU_LIST_FOR(entry, obj_material_entry, &material_head)) {
	if (BU_STR_EQUAL(entry->name, name))
	    return 1;
    }

    return 0;
}


static void
obj_write_material_definition(const char *name, int color_valid, const unsigned char rgb[3])
{
    struct obj_material_entry *entry;
    double kd[3] = {0.8, 0.8, 0.8};

    if (!mtl_fp || !name || obj_material_exists(name))
	return;

    if (color_valid) {
	kd[0] = ((double)rgb[0]) / 255.0;
	kd[1] = ((double)rgb[1]) / 255.0;
	kd[2] = ((double)rgb[2]) / 255.0;
    }

    fprintf(mtl_fp, "newmtl %s\n", name);
    fprintf(mtl_fp, "Ka %.6f %.6f %.6f\n", kd[0], kd[1], kd[2]);
    fprintf(mtl_fp, "Kd %.6f %.6f %.6f\n", kd[0], kd[1], kd[2]);
    fprintf(mtl_fp, "Ks 0.000000 0.000000 0.000000\n");
    fprintf(mtl_fp, "d 1.000000\n");
    fprintf(mtl_fp, "illum 1\n\n");

    BU_ALLOC(entry, struct obj_material_entry);
    BU_LIST_INIT(&entry->l);
    entry->name = bu_strdup(name);
    BU_LIST_APPEND(&material_head, &entry->l);
}


static void
obj_cleanup_materials(void)
{
    struct obj_material_entry *entry;

    if (mtl_fp) {
	fclose(mtl_fp);
	mtl_fp = NULL;
    }

    while (BU_LIST_WHILE(entry, obj_material_entry, &material_head)) {
	BU_LIST_DEQUEUE(&entry->l);
	bu_free(entry->name, "obj material name");
	bu_free(entry, "obj material entry");
    }

    bu_vls_free(&material_lib_name);
}


static void
obj_open_material_file(const char *obj_path)
{
    struct bu_vls dir = BU_VLS_INIT_ZERO;
    struct bu_vls base = BU_VLS_INIT_ZERO;
    struct bu_vls mtl_path = BU_VLS_INIT_ZERO;

    if (!obj_path || !strlen(obj_path))
	goto cleanup;

    if (!bu_path_component(&base, obj_path, BU_PATH_BASENAME_EXTLESS))
	goto cleanup;

    bu_vls_sprintf(&material_lib_name, "%s.mtl", bu_vls_addr(&base));

    if (bu_path_component(&dir, obj_path, BU_PATH_DIRNAME) &&
	    bu_vls_strlen(&dir) > 0 &&
	    !BU_STR_EQUAL(bu_vls_addr(&dir), ".")) {
	bu_vls_sprintf(&mtl_path, "%s/%s", bu_vls_addr(&dir), bu_vls_addr(&material_lib_name));
    } else {
	bu_vls_sprintf(&mtl_path, "%s", bu_vls_addr(&material_lib_name));
    }

    if ((mtl_fp = fopen(bu_vls_addr(&mtl_path), "wb")) == NULL) {
	bu_log("Cannot open companion material file (%s) for writing\n", bu_vls_addr(&mtl_path));
	perror("g-obj");
	bu_vls_trunc(&material_lib_name, 0);
	goto cleanup;
    }

    fprintf(mtl_fp, "# BRL-CAD generated Wavefront material file\n");

cleanup:
    bu_vls_free(&dir);
    bu_vls_free(&base);
    bu_vls_free(&mtl_path);
}


static void
obj_write_region_material(const struct db_full_path *pathp, const struct db_tree_state *tsp)
{
    struct bu_vls material_name = BU_VLS_INIT_ZERO;
    unsigned char rgb[3] = {0, 0, 0};
    struct db_i *idbip = tsp->ts_dbip ? tsp->ts_dbip : dbip;
    int color_valid = obj_get_path_color(idbip, pathp, rgb);

    if (!color_valid)
	color_valid = obj_get_export_color(idbip, tsp->ts_regionid, &tsp->ts_mater, rgb);

    if (mtl_fp) {
	if (obj_material_name(&material_name, color_valid, rgb, usemtl,
		    tsp->ts_aircode, tsp->ts_los, tsp->ts_gmater)) {
	    obj_write_material_definition(bu_vls_addr(&material_name), color_valid, rgb);
	    fprintf(fp, "usemtl %s\n", bu_vls_addr(&material_name));
	}
	bu_vls_free(&material_name);
	return;
    }

    if (color_valid && !warned_missing_mtl) {
	bu_log("g-obj: unable to export BRL-CAD colors without a companion .mtl file; continuing without color assignments\n");
	warned_missing_mtl = 1;
    }

    if (usemtl)
	fprintf(fp, "usemtl mat_%d_%d_%d\n", tsp->ts_gmater, tsp->ts_los, tsp->ts_aircode);
}

static int
parse_tol_abs(struct bu_vls *error_msg, size_t argc, const char **argv, void *set_var)
{
    int ret;
    BU_OPT_CHECK_ARGV0(error_msg, argc, argv, "absolute tolerance");

    if (set_var) {
	ret = bu_opt_fastf_t(error_msg, argc, argv, (void *)&(ttol.abs));
	ttol.rel = 0.0;
	return ret;
    } else {
	return -1;
    }
}


static int
parse_tol_norm(struct bu_vls *error_msg, size_t argc, const char **argv, void *set_var)
{
    int ret;
    BU_OPT_CHECK_ARGV0(error_msg, argc, argv, "normal tolerance");

    if (set_var) {
	ret = bu_opt_fastf_t(error_msg, argc, argv, (void *)&(ttol.norm));
	ttol.rel = 0.0;
	return ret;
    } else {
	return -1;
    }
}


static int
parse_tol_dist(struct bu_vls *error_msg, size_t argc, const char **argv, void *set_var)
{
    int ret;
    BU_OPT_CHECK_ARGV0(error_msg, argc, argv, "distance tolerance");

    if (set_var) {
	ret = bu_opt_fastf_t(error_msg, argc, argv, (void *)&(tol.dist));
	tol.dist_sq = tol.dist * tol.dist;
	rt_pr_tol(&tol);
	return ret;
    } else {
	return -1;
    }
}


static int
parse_debug_rt(struct bu_vls *error_msg, size_t argc, const char **argv, void *set_var)
{
    BU_OPT_CHECK_ARGV0(error_msg, argc, argv, "debug rt");

    if (set_var) {
	sscanf(argv[0], "%x", (unsigned int *)&rt_debug);
	return 1;
    } else {
	return -1;
    }
}


static int
parse_debug_nmg(struct bu_vls *error_msg, size_t argc, const char **argv, void *set_var)
{
    BU_OPT_CHECK_ARGV0(error_msg, argc, argv, "debug nmg");

    if (set_var) {
	sscanf(argv[0], "%x", (unsigned int *)&nmg_debug);
	NMG_debug = nmg_debug;
	return 1;
    } else {
	return -1;
    }
}

int
main(int argc, const char **argv)
{
    int c;
    double percent;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    const char *prog_name = argv[0];
    struct bu_opt_desc options[16];
    BU_OPT(options[0], "?", "", NULL,          NULL,            &print_help,  "print help and exit");
    BU_OPT(options[1], "h", "", NULL,          NULL,            &print_help,  "print help and exit");
    BU_OPT(options[2], "i", "", NULL,          NULL,            &inches,      "change output units from mm to inches");
    BU_OPT(options[3], "m", "", NULL,          NULL,            &usemtl,      "output usemtl statements");
    BU_OPT(options[4], "u", "", NULL,          NULL,            &do_normals,  "output vertex normals");
    BU_OPT(options[5], "v", "", NULL,          NULL,            &verbose,     "verbose output");
    BU_OPT(options[6], "a", "", "#",           parse_tol_abs,   &ttol,        "absolute tolerance");
    BU_OPT(options[7], "n", "", "#",           parse_tol_norm,  &ttol,        "surface normal tolerance");
    BU_OPT(options[8], "D", "", "#",           parse_tol_dist,  &tol,         "distance tolerance");
    BU_OPT(options[9], "x", "", "level",       parse_debug_rt,  &rt_debug,   "set RT debug flag");
    BU_OPT(options[10], "X", "", "level",      parse_debug_nmg, &nmg_debug,   "set NMG debug flag");
    BU_OPT(options[11], "e", "", "error_file", bu_opt_str,      &error_file,  "error file name");
    BU_OPT(options[12], "o", "", "output.obj", bu_opt_str,      &output_file, "output file name");
    BU_OPT(options[13], "P", "", "#",          bu_opt_int,      &ncpu,        "number of CPUs");
    BU_OPT(options[14], "r", "", "#",          bu_opt_fastf_t,  &ttol.rel,    "relative tolerance");
    BU_OPT_NULL(options[15]);

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    RT_DBTS_INIT(&tree_state);
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;
    BU_LIST_INIT(&material_head);

    ttol.magic = BG_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    the_model = nmg_mm();
    struct bu_list *vlfree = &rt_vlfree;

    /* Get command line arguments. */
    ++argv; --argc;

    argc = bu_opt_parse(&parse_msgs, argc, argv, options);

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_cstr(&parse_msgs));
    }
    if (argc < 2 || print_help) {
	print_usage(prog_name);
    }

    if (!output_file)
	fp = stdout;
    else {
	/* Open output file */
	if ((fp=fopen(output_file, "wb+")) == NULL) {
	    perror(argv[0]);
	    bu_exit(1, "Cannot open output file (%s) for writing\n", output_file);
	}
    }

    obj_open_material_file(output_file);

    /* Open g-obj error log file */
    if (!error_file) {
	fpe = stderr;
	setmode(fileno(fpe), O_BINARY);
    } else if ((fpe=fopen(error_file, "wb")) == NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open output file (%s) for writing\n", error_file);
    }

    /* Open BRL-CAD database */
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry database file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip))
	bu_exit(1, "db_dirbuild failed\n");

    BN_CK_TOL(tree_state.ts_tol);
    BG_CK_TESS_TOL(tree_state.ts_ttol);

    /* Write out header */
    if (inches)
	fprintf(fp, "# BRL-CAD generated Wavefront OBJ file (Units in)\n");
    else
	fprintf(fp, "# BRL-CAD generated Wavefront OBJ file (Units mm)\n");
    if (mtl_fp)
	fprintf(fp, "mtllib %s\n", bu_vls_addr(&material_lib_name));

    fprintf(fp, "# BRL-CAD model: %s\n# BRL_CAD objects:", argv[0]);

    for (c=1; c<argc; c++)
	fprintf(fp, " %s", argv[c]);
    fprintf(fp, "\n");

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			do_region_end,
			rt_booltree_leaf_tess,
			(void *)vlfree);	/* in librt/nmg_bool.c */

    if (regions_tried>0) {
	percent = ((double)regions_converted * 100.0) / regions_tried;
	bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
	       regions_tried, regions_converted, percent);
	percent = ((double)regions_written * 100.0) / regions_tried;
	bu_log("                 %d triangulated successfully. %g%%\n",
	       regions_written, percent);
    }

    fclose(fp);
    obj_cleanup_materials();

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

    return 0;
}


static void
nmg_to_obj(struct nmgregion *r, const struct db_full_path *pathp, const struct db_tree_state *tsp, struct bu_list *vlfree)
{
    struct model *m = NULL;
    struct shell *s = NULL;
    struct vertex *v = NULL;
    struct bu_ptbl verts = BU_PTBL_INIT_ZERO;
    struct bu_ptbl norms = BU_PTBL_INIT_ZERO;
    char *region_name = NULL;
    size_t numverts = 0;		/* Number of vertices to output */
    size_t i = 0;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, vlfree, &tol);

    /* list all vertices in result */
    nmg_vertex_tabulate(&verts, &r->l.magic, vlfree);

    /* Get number of vertices */
    numverts = BU_PTBL_LEN(&verts);

    /* get list of vertexuse normals */
    if (do_normals)
	nmg_vertexuse_normal_tabulate(&norms, &r->l.magic, vlfree);

    /* BEGIN CHECK SECTION */
    /* Check vertices */

    for (i=0; i<numverts; i++) {
	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);
    }

    /* Check triangles */
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;
		int vert_count=0;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    int loc;
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    vert_count++;
		    loc = bu_ptbl_locate(&verts, (long *)v);
		    if (loc < 0) {
			bu_ptbl_free(&verts);
			bu_free(region_name, "region name");
			bu_log("Vertex from eu %p is not in nmgregion %p\n", (void *)eu, (void *)r);
			bu_exit(1, "ERROR: Can't find vertex in list!");
		    }
		}
		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle\n");
		} else if (vert_count < 3)
		    continue;
	    }
	}
    }

    /* END CHECK SECTION */
    /* Write pertinent info for this region */

    obj_write_region_material(pathp, tsp);

    fprintf(fp, "g %s", pathp->fp_names[0]->d_namep);
    for (i=1; i<pathp->fp_len; i++)
	fprintf(fp, "/%s", pathp->fp_names[i]->d_namep);
    fprintf(fp, "\n");

    /* Write vertices */
    for (i=0; i<numverts; i++) {
	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);
	if (inches)
	    fprintf(fp, "v %f %f %f\n", V3ARGSIN(v->vg_p->coord));
	else
	    fprintf(fp, "v %f %f %f\n", V3ARGS(v->vg_p->coord));
    }

    /* Write vertexuse normals */
    if (do_normals) {
	for (i=0; i<BU_PTBL_LEN(&norms); i++) {
	    struct vertexuse_a_plane *va;

	    va = (struct vertexuse_a_plane *)BU_PTBL_GET(&norms, i);
	    NMG_CK_VERTEXUSE_A_PLANE(va);
	    if (inches)
		fprintf(fp, "vn %f %f %f\n", V3ARGSIN(va->N));
	    else
		fprintf(fp, "vn %f %f %f\n", V3ARGS(va->N));
	}
    }

    /* output triangles */
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;
		int vert_count=0;
		int use_normals=1;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* Each vertexuse of the face must have a normal in order
		 * to use the normals in Wavefront
		 */
		if (do_normals) {
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);

			if (!eu->vu_p->a.magic_p) {
			    use_normals = 0;
			    break;
			}

			if (*eu->vu_p->a.magic_p != NMG_VERTEXUSE_A_PLANE_MAGIC) {
			    use_normals = 0;
			    break;
			}
		    }
		} else
		    use_normals = 0;

		fprintf(fp, "f");

		/* list vertex numbers for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    int loc;
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    vert_count++;
		    loc = bu_ptbl_locate(&verts, (long *)v);
		    if (loc < 0) {
			bu_ptbl_free(&verts);
			bu_log("Vertex from eu %p is not in nmgregion %p\n", (void *)eu, (void *)r);
			bu_free(region_name, "region name");
			bu_exit(1, "Can't find vertex in list!\n");
		    }

		    if (use_normals) {
			int j;

			j = bu_ptbl_locate(&norms, (long *)eu->vu_p->a.magic_p);
			fprintf(fp, " %ld//%ld", loc+1+(long)vert_offset, j+1+(long)norm_offset);
		    } else
			fprintf(fp, " %ld", loc+1+(long)vert_offset);
		}

		fprintf(fp, "\n");

		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle\n");
		}
	    }
	}
    }
    vert_offset += numverts;
    bu_ptbl_free(&verts);
    if (do_normals) {
	norm_offset += BU_PTBL_LEN(&norms);
	bu_ptbl_free(&norms);
    }
    bu_free(region_name, "region name");
}


static void
process_triangulation(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp, struct bu_list *vlfree)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the region to the TANKILL file */
	nmg_to_obj(r, pathp, tsp, vlfree);

    } else {
	/* catch */

	char *sofar;

	sofar = db_path_to_string(pathp);
	bu_log("FAILED in triangulator: %s\n", sofar);
	bu_free((char *)sofar, "sofar");

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before bombing out.
	 */
	nmg_debug = NMG_debug;	/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Get rid of (m)any other intermediate structures */
	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
    }  BU_UNSETJUMP;
}


static union tree *
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp, struct bu_list *vlfree)
{
    static union tree *ret_tree = TREE_NULL;

    /* Begin bomb protection */
    if (!BU_SETJUMP) {
	/* try */

	(void)nmg_model_fuse(*tsp->ts_m, vlfree, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(curtree, vlfree, tsp->ts_tol);

    } else {
	/* catch */
	char *name = db_path_to_string(pathp);

	/* Error, bail out */
	bu_log("conversion of %s FAILED!\n", name);

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before before bombing out.
	 */
	nmg_debug = NMG_debug;/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Release the tree memory & input regions */
	db_free_tree(curtree);/* Does an nmg_kr() */

	/* Get rid of (m)any other intermediate structures */
	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	bu_free(name, "db_path_to_string");
	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
    } BU_UNSETJUMP;/* Relinquish the protection */

    return ret_tree;
}


/*
 * Called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    union tree *ret_tree;
    struct bu_list vhead;
    struct nmgregion *r;
    struct bu_list *vlfree = (struct bu_list *)client_data;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    BG_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK || verbose) {
	char *sofar = db_path_to_string(pathp);
	bu_log("\ndo_region_end(%d %d%%) %s\n",
	       regions_tried,
	       regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
	       sofar);
	bu_free(sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP)
	return curtree;

    regions_tried++;

    ret_tree = process_boolean(curtree, tsp, pathp, vlfree);

    if (ret_tree)
	r = ret_tree->tr_d.td_r;
    else
	r = (struct nmgregion *)NULL;

    regions_converted++;

    if (r != 0) {
	struct shell *s;
	int empty_region=0;
	int empty_model=0;

	/* Kill cracks */
	s = BU_LIST_FIRST(shell, &r->s_hd);
	while (BU_LIST_NOT_HEAD(&s->l, &r->s_hd)) {
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT(shell, &s->l);
	    if (nmg_kill_cracks(s)) {
		if (nmg_ks(s)) {
		    empty_region = 1;
		    break;
		}
	    }
	    s = next_s;
	}

	/* kill zero length edgeuses */
	if (!empty_region) {
	    empty_model = nmg_kill_zero_length_edgeuses(*tsp->ts_m);
	}

	if (!empty_region && !empty_model) {
	    process_triangulation(r, pathp, tsp, vlfree);

	    regions_written++;

	    BU_UNSETJUMP;
	}

	if (!empty_model)
	    nmg_kr(r);
    }

    /* Dispose of original tree, so that all associated dynamic memory
     * is released now, not at the end of all regions.  A return of
     * TREE_NULL from this routine signals an error, and there is no
     * point to adding _another_ message to our output, so we need to
     * cons up an OP_NOP node to return.
     */


    db_free_tree(curtree);		/* Does an nmg_kr() */

    if (regions_tried>0) {
	float npercent;
	float tpercent;

	npercent = (float)(regions_converted * 100) / regions_tried;
	tpercent = (float)(regions_written * 100) / regions_tried;
	bu_log("Tried %d regions; %d conv. to NMG's, %d conv. to tri.; nmgper = %.2f%%, triper = %.2f%%\n",
	       regions_tried, regions_converted, regions_written, npercent, tpercent);
    }

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
