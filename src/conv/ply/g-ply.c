/*                  G - P L Y . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2021 United States Government as represented by
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
/** @file conv/ply/g-ply.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to the Stanford
 * PLY format by calling on the NMG booleans. Based on g-stl.c.
 *
 */

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/parallel.h"
#include "bu/hash.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "rply.h"


static int NMG_debug = 0; /* saved arg of -X, for longjmp handling */
static int verbose = 0;
static struct db_i *dbip = NULL;
static struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO; /* tessellation tolerance in mm */
static struct bn_tol tol = BN_TOL_INIT_ZERO;            /* calculation tolerance */
static struct model *the_model = NULL;
static char *ply_file = NULL;

static long ***f_regs = NULL;
static double ***v_regs = NULL;
static struct bu_hash_tbl **v_tbl_regs = NULL;
static int *f_sizes = NULL;

static int v_order = 0;
static int merge_all = 1;
static int storage_type = 0;
static int all_colors[3] = {0, 0, 0};
static int color_info = 0; /* 0: not checked yet, 1: all same so far, 2: no color information, 3: not same color */

static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;
static int cur_region = 0;
static int tot_regions = 0;
static long tot_polygons = 0;
static long tot_vertices = 0;


static struct bu_hash_entry *
write_verts(p_ply fp, struct bu_hash_tbl *t)
{
    struct bu_hash_entry *v_entry = bu_hash_next(t, NULL);
    while (v_entry) {
	/* TODO: determine if possible with new libbu hash API? */
	double *coords = (double *)(bu_hash_value(v_entry, NULL));
	if (!coords) return v_entry;

	/* PLY files are usually in meters */
	ply_write(fp, coords[0] / 1000);
	ply_write(fp, coords[1] / 1000);
	ply_write(fp, coords[2] / 1000);

	/* keeping track of the order in which the vertices are input,
	 * to write the faces
	 */
	coords[3] = v_order;
	v_order++;

	v_entry = bu_hash_next(t, v_entry);
    }
    return NULL;
}


/* routine to output the faceted NMG representation of a BRL-CAD
 * region
 */
static void
nmg_to_ply(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id), struct db_tree_state *tsp)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name = NULL;
    long nvertices = 0;
    long nfaces = 0;
    int color[3];
    int reg_faces_pos = 0;
    p_ply ply_fp = NULL;

    VSCALE(color, tsp->ts_mater.ma_color, 255);
    if (merge_all) {
	if (!tsp->ts_mater.ma_color_valid) {
	    color_info = 2;
	} else if (color_info == 0) {
	    all_colors[0] = color[0];
	    all_colors[1] = color[1];
	    all_colors[2] = color[2];
	    color_info = 1;
	} else if (color_info == 1 && (all_colors[0] != color [0] || all_colors[1] != color [1] || all_colors[2] != color [2])) {
	    color_info = 3;
	}
    }
    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    if (!merge_all) {
	/* Open output file */
	char *comment;
	unsigned int k;
	char *region_file;

	region_file = (char *) bu_calloc(strlen(region_name) + strlen(ply_file), sizeof(char), "region_file");
	sprintf(region_file, "%s_%s", region_name+1, ply_file);
	for (k = 0; k < strlen(region_file); k++) {
	    switch (region_file[k]) {
		case '/':
		    region_file[k] = '_';
		    break;
	    }
	}

	switch (storage_type) {
	    case 0:
		ply_fp = ply_create(region_file, PLY_ASCII, NULL, 0, NULL);
		break;
	    case 1:
		ply_fp = ply_create(region_file, PLY_LITTLE_ENDIAN, NULL, 0, NULL);
		break;
	    case 2:
		ply_fp = ply_create(region_file, PLY_BIG_ENDIAN, NULL, 0, NULL);
		break;
	    case 3:
		ply_fp = ply_create(region_file, PLY_DEFAULT, NULL, 0, NULL);
		break;
	}
	if (!ply_fp) {
	    bu_hash_destroy(v_tbl_regs[cur_region]);
	    bu_free(region_file, "region_file");
	    bu_log("ERROR: Unable to create PLY file");
	    goto free_nmg;
	}
	if (!ply_add_comment(ply_fp, "converted from BRL-CAD")) {
	    bu_hash_destroy(v_tbl_regs[cur_region]);
	    bu_free(region_file, "region_file");
	    bu_log("ERROR: Unable to write to PLY file");
	    goto free_nmg;
	}

	comment = (char *) bu_calloc(strlen(region_name) + 14, sizeof(char), "comment");
	sprintf(comment, "wrote region %s", (region_name+1));
	ply_add_comment(ply_fp, comment);
	bu_free(comment, "comment");
	bu_free(region_file, "region_file");
    }

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, &RTG.rtg_vlfree, &tol);

    /* Output triangles */
    if (verbose)
	bu_log("Converting triangles to PLY format for region %s\n", region_name);

    /* count number of faces for hash bin size */
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;
	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		nfaces++;
	    }

	}
    }

    /* RPly library cannot handle faces and vertices greater than the
     * integer limit
     */
    if (nfaces >= INT_MAX) {
	bu_hash_destroy(v_tbl_regs[cur_region]);
	bu_log("ERROR: Number of faces (%ld) exceeds integer limit!\n", nfaces);
	goto free_nmg;
    }

    f_sizes[cur_region] = (int) nfaces;
    v_tbl_regs[cur_region] = bu_hash_create(nfaces * 3);
    f_regs[cur_region] = (long **) bu_calloc(nfaces, sizeof(long *), "reg_faces");
    v_regs[cur_region] = (double **) bu_calloc(nfaces * 3, sizeof(double *), "reg_verts"); /* chose to do this over dynamic array, but I may be wrong */

    /* count number of vertices and put them into the hash table */
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
		int v_ind_pos = 0;
		f_regs[cur_region][reg_faces_pos] = (long *) bu_calloc(3, sizeof(long), "v_ind");
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		if (verbose)
		    bu_log("\tfacet:\n");
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    f_regs[cur_region][reg_faces_pos][v_ind_pos] = v->index;
		    if (verbose)
			bu_log("\t\t(%g %g %g)\n", V3ARGS(v->vg_p->coord));

		    if (!bu_hash_get(v_tbl_regs[cur_region], (const uint8_t *)(f_regs[cur_region][reg_faces_pos] + v_ind_pos), sizeof(long *))) {
			v_regs[cur_region][nvertices] = (double *) bu_calloc(4, sizeof(double), "v_coords");
			v_regs[cur_region][nvertices][0] = (double)(v->vg_p->coord[0]);
			v_regs[cur_region][nvertices][1] = (double)(v->vg_p->coord[1]);
			v_regs[cur_region][nvertices][2] = (double)(v->vg_p->coord[2]);
			(void)bu_hash_set(v_tbl_regs[cur_region], (const uint8_t *)(f_regs[cur_region][reg_faces_pos] + v_ind_pos), sizeof(long *), (void *)v_regs[cur_region][nvertices]);
			nvertices++;
		    }

		    if (v_ind_pos > 2) {
			bu_log("ERROR: Too many vertices per face!\n");
			goto free_nmg;
		    }
		    f_regs[cur_region][reg_faces_pos][v_ind_pos] = v->index;
		    v_ind_pos++;
		}
		if (reg_faces_pos >= nfaces) {
		    bu_log("ERROR: More loopuses than faces!\n");
		    goto free_nmg;
		}
		reg_faces_pos++;
	    }
	}
    }

    if (nvertices >= INT_MAX) {
	bu_hash_destroy(v_tbl_regs[cur_region]);
	bu_log("ERROR: Number of vertices (%ld) exceeds integer limit!\n", nvertices);
	goto free_nmg;
    }

    if (!merge_all) {
	int fi;
	int vi;
	double *coords;

	ply_add_element(ply_fp, "vertex", nvertices);
	ply_add_scalar_property(ply_fp, "x", PLY_FLOAT);
	ply_add_scalar_property(ply_fp, "y", PLY_FLOAT);
	ply_add_scalar_property(ply_fp, "z", PLY_FLOAT);
	ply_add_element(ply_fp, "face", nfaces);
	ply_add_list_property(ply_fp, "vertex_indices", PLY_UCHAR, PLY_UINT);
	if (tsp->ts_mater.ma_color_valid) {
	    ply_add_scalar_property(ply_fp, "red", PLY_UCHAR);
	    ply_add_scalar_property(ply_fp, "green", PLY_UCHAR);
	    ply_add_scalar_property(ply_fp, "blue", PLY_UCHAR);
	}
	ply_write_header(ply_fp);

	if (write_verts(ply_fp, v_tbl_regs[cur_region])) {
	    bu_hash_destroy(v_tbl_regs[cur_region]);
	    bu_log("ERROR: No coordinates found for vertex!\n");
	    goto free_nmg;
	}
	for (fi = 0; fi < (int) nfaces; fi++) {
	    ply_write(ply_fp, 3);
	    for (vi = 0; vi < 3; vi++) {
		coords = (double *)bu_hash_get(v_tbl_regs[cur_region], (const unsigned char *)(f_regs[cur_region][fi] + vi), sizeof(long));
		if (!coords) {
		    bu_log("ERROR: No vertex found for face with index %ld!\n", f_regs[cur_region][fi][vi]);
		    goto free_nmg;
		}
		ply_write(ply_fp, coords[3]);
	    }
	    if(tsp->ts_mater.ma_color_valid) {
		ply_write(ply_fp, color[0]);
		ply_write(ply_fp, color[1]);
		ply_write(ply_fp, color[2]);
	    }
	    bu_free(f_regs[cur_region][fi], "v_ind");
	}

	v_order = 0;
	ply_close(ply_fp);
    }

    tot_polygons += nfaces;
    tot_vertices += nvertices;

    if (!merge_all) {
	bu_hash_destroy(v_tbl_regs[cur_region]);
	bu_free(f_regs[cur_region], "reg_faces");
    }
    bu_free(region_name, "region_name");
    cur_region++;
    return;

free_nmg:
    if (ply_fp)
	ply_close(ply_fp);
    if (region_name)
	bu_free(region_name, "region_name");
    if (f_regs)
	bu_free(f_regs, "f_regs");
    if (f_sizes)
	bu_free(f_sizes, "f_sizes");
    if (v_regs)
	bu_free(v_regs, "v_regs");
    if (v_tbl_regs)
	bu_free(v_tbl_regs, "v_tbl_regs");
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);
    bu_exit(1, NULL);
}


static void
process_triangulation(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the facetized region to the output file */
	nmg_to_ply(r, pathp, tsp->ts_regionid, tsp->ts_gmater, tsp);

    } else {
	/* catch */

	char *sofar;

	sofar = db_path_to_string(pathp);
	bu_log("FAILED in triangulator: %s\n", sofar);
	bu_free((char *)sofar, "sofar");

	/* Sometimes the NMG library adds debugging bits when it
	 * detects an internal error, before bombing out.
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
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp)
{
    static union tree *ret_tree = TREE_NULL;

    /* Begin bomb protection */
    if (!BU_SETJUMP) {
	/* try */

	(void)nmg_model_fuse(*tsp->ts_m, &RTG.rtg_vlfree, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(curtree, &RTG.rtg_vlfree, tsp->ts_tol, &rt_uniresource);

    } else {
	/* catch */
	char *name = db_path_to_string(pathp);

	/* Error, bail out */
	bu_log("conversion of %s FAILED!\n", name);

	/* Sometimes the NMG library adds debugging bits when it
	 * detects an internal error, before before bombing out.
	 */
	nmg_debug = NMG_debug;/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Release the tree memory & input regions */
	db_free_tree(curtree, &rt_uniresource);/* Does an nmg_kr() */

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
static union tree *
do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *UNUSED(client_data))
{
    union tree *ret_tree;
    struct bu_list vhead;
    struct nmgregion *r;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    {
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

    if (verbose)
	bu_log("Attempting to process region %s\n", db_path_to_string(pathp));

    ret_tree= process_boolean(curtree, tsp, pathp);

    if (ret_tree)
	r = ret_tree->tr_d.td_r;
    else
    {
	if (verbose)
	    bu_log("\tNothing left of this region after Boolean evaluation\n");
	regions_written++; /* don't count as a failure */
	r = (struct nmgregion *)NULL;
    }

    regions_converted++;

    if (r != (struct nmgregion *)NULL)
    {
	struct shell *s;
	int empty_region=0;
	int empty_model=0;

	/* Kill cracks */
	s = BU_LIST_FIRST(shell, &r->s_hd);
	while (BU_LIST_NOT_HEAD(&s->l, &r->s_hd))
	{
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT(shell, &s->l);
	    if (nmg_kill_cracks(s))
	    {
		if (nmg_ks(s))
		{
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
	    process_triangulation(r, pathp, tsp);

	    regions_written++;
	}

	if (!empty_model)
	    nmg_kr(r);
    }

    /*
     * Dispose of original tree, so that all associated dynamic memory
     * is released now, not at the end of all regions.  A return of
     * TREE_NULL from this routine signals an error, and there is no
     * point to adding _another_ message to our output, so we need to
     * cons up an OP_NOP node to return.
     */


    db_free_tree(curtree, &rt_uniresource); /* Does an nmg_kr() */

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}


static int
count_regions(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), const struct rt_comb_internal *UNUSED(combp), void *UNUSED(client_data))
{
    tot_regions++;
    return -1;
}


/* taken from g-tankill.c */
static union tree *
region_stub(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), union tree *UNUSED(curtree), void *UNUSED(client_data))
{
    bu_exit(1, "ERROR; region stub called, this shouldn't happen.\n");
    return (union tree *)NULL; /* just to keep the compilers happy */
}


/* taken from g-tankill.c */
static union tree *
leaf_stub(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), struct rt_db_internal *UNUSED(ip), void *UNUSED(client_data))
{
    bu_exit(1, "ERROR: leaf stub called, this shouldn't happen.\n");
    return (union tree *)NULL; /* just to keep the compilers happy */
}


int
main(int argc, char **argv)
{
    static char usage[] = "\
Usage: %s [-v][-xX lvl][-a abs_tess_tol (default: 0.0)][-r rel_tess_tol (default: 0.01)]\n\
  [-n norm_tess_tol (default: 0.0)][-t type (asc: ascii), (le: little endian), (be: big endian)]\n\
  [-s separate file per object][-D dist_calc_tol (default: 0.0005)] -o output_file_name brlcad_db.g object(s)\n";

    int c, j;
    double percent;
    p_ply ply_fp = NULL;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;

    /* Set up tessellation tolerance defaults */
    ttol.magic = BG_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* Set up calculation tolerance defaults */
    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    /* init resources we might need */
    rt_init_resource(&rt_uniresource, 0, NULL);

    /* make empty NMG model */
    the_model = nmg_mm();

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "r:a:n:o:t:svx:D:X:h?")) != -1) {
	switch (c) {
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'o':		/* Output file name. */
		ply_file = bu_optarg;
		break;
	    case 't':
		if (!bu_strcmp(bu_optarg, "asc"))
		    storage_type = 0;
		else if (!bu_strcmp(bu_optarg, "le"))
		    storage_type = 1;
		else if (!bu_strcmp(bu_optarg, "be"))
		    storage_type = 2;
		else if (!bu_strcmp(bu_optarg, "de"))
		    storage_type = 3;
		break;
	    case 's':		/* Merge all objects into one ply file */
		merge_all = 0;
		break;
	    case 'v':
		verbose++;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_debug);
		break;
	    case 'D':
		tol.dist = atof(bu_optarg);
		tol.dist_sq = tol.dist * tol.dist;
		rt_pr_tol(&tol);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&nmg_debug);
		NMG_debug = nmg_debug;
		break;
	    default:
		bu_exit(1, usage, argv[0]);
	}
    }

    if (bu_optind+1 >= argc)
	bu_exit(1, usage, argv[0]);

    if (merge_all) {
	/* Open output file */
	switch (storage_type) {
	    case 0:
		ply_fp = ply_create(ply_file, PLY_ASCII, NULL, 0, NULL);
		break;
	    case 1:
		ply_fp = ply_create(ply_file, PLY_LITTLE_ENDIAN, NULL, 0, NULL);
		break;
	    case 2:
		ply_fp = ply_create(ply_file, PLY_BIG_ENDIAN, NULL, 0, NULL);
		break;
	    case 3:
		ply_fp = ply_create(ply_file, PLY_DEFAULT, NULL, 0, NULL);
		break;
	}

	if (!ply_fp) {
	    ply_close(ply_fp);
	    nmg_km(the_model);
	    rt_vlist_cleanup();
	    bu_exit(1, "ERROR: Unable to create PLY file");
	}
	if (!ply_add_comment(ply_fp, "converted from BRL-CAD")) {
	    ply_close(ply_fp);
	    nmg_km(the_model);
	    rt_vlist_cleanup();
	    bu_exit(1, "ERROR: Unable to write to PLY file");
	}

	all_colors[0] = -1;
	all_colors[1] = -1;
	all_colors[2] = -1;
    }

    /* Open BRL-CAD database */
    argc -= bu_optind;
    argv += bu_optind;
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	ply_close(ply_fp);
	nmg_km(the_model);
	rt_vlist_cleanup();
	bu_exit(1, "ERROR: Unable to open geometry database file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip)) {
	ply_close(ply_fp);
	nmg_km(the_model);
	rt_vlist_cleanup();
	bu_exit(1, "db_dirbuild failed\n");
    }
    BN_CK_TOL(tree_state.ts_tol);
    BG_CK_TESS_TOL(tree_state.ts_ttol);

    if (verbose) {
	int i;

	bu_log("Model: %s\n", argv[0]);
	bu_log("Objects:");
	for (i = 1; i < argc; i++)
	    bu_log(" %s", argv[i]);
	bu_log("\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	       tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
	bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	       tree_state.ts_tol->dist, tree_state.ts_tol->perp);
    }

    /* Verify that all the specified objects are valid - if one or
     * more of them are not, bail
     */
    for (j = 0; j < argc - 1; j++) {
	struct directory *dp = db_lookup(dbip, argv[1+j], LOOKUP_NOISY);
	if (dp == RT_DIR_NULL) {
	    bu_exit(1, "invalid object\n");
	}
    }

    /* Quickly get number of regions. I did this over dynamically
     * allocating the memory, but that may be wrong
     */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,		/* ncpu */
			&tree_state,
			count_regions,
			region_stub,
			leaf_stub,
			(void *)NULL);	/* in librt/nmg_bool.c */
    if (verbose)
	bu_log("Found %d total regions.\n", tot_regions);

    f_regs = (long ***) bu_calloc(tot_regions, sizeof(long**), "f_regs");
    f_sizes = (int *) bu_calloc(tot_regions, sizeof(int), "f_sizes");
    v_regs = (double ***) bu_calloc(tot_regions, sizeof(double**), "v_regs");
    v_tbl_regs = (struct bu_hash_tbl **) bu_calloc(tot_regions, sizeof(struct bu_hash_tbl*), "v_tbl_regs");

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,		/* ncpu */
			&tree_state,
			0,		/* take all regions */
			do_region_end,
			nmg_booltree_leaf_tess,
			(void *)NULL);	/* in librt/nmg_bool.c */
    if (verbose)
	bu_log("Found %d nmg regions.\n", cur_region);

    if (merge_all) {
	int ri;
        int fi;
        int vi;
	double *coords;

        ply_add_element(ply_fp, "vertex", tot_vertices);
        ply_add_scalar_property(ply_fp, "x", PLY_FLOAT);
        ply_add_scalar_property(ply_fp, "y", PLY_FLOAT);
        ply_add_scalar_property(ply_fp, "z", PLY_FLOAT);
        ply_add_element(ply_fp, "face", tot_polygons);
        ply_add_list_property(ply_fp, "vertex_indices", PLY_UCHAR, PLY_UINT);
        if (color_info == 1) {
            ply_add_scalar_property(ply_fp, "red", PLY_UCHAR);
            ply_add_scalar_property(ply_fp, "green", PLY_UCHAR);
            ply_add_scalar_property(ply_fp, "blue", PLY_UCHAR);
        }
        ply_write_header(ply_fp);

	for (ri = 0; ri < cur_region; ri++)
	    if (write_verts(ply_fp, v_tbl_regs[ri])) {
		bu_log("ERROR: No coordinates found for vertex!\n");
		ply_close(ply_fp);
		goto free_all;
	    }
	for (ri = 0; ri < cur_region; ri++) {
	    for (fi = 0; fi < f_sizes[ri]; fi++) {
		ply_write(ply_fp, 3);
		for (vi = 0; vi < 3; vi++) {
		    coords = (double *)bu_hash_get(v_tbl_regs[ri], (const unsigned char *)(f_regs[ri][fi] + vi), sizeof(long));
		    if (!coords) {
			bu_log("ERROR: No vertex found for face with index %ld!\n", f_regs[ri][fi][vi]);
			ply_close(ply_fp);
			goto free_all;
		    }
		    ply_write(ply_fp, coords[3]);
		}
		if (color_info == 0) {
		    bu_log("ERROR: Could not find any regions");
		    ply_close(ply_fp);
		    goto free_all;
		}
		else if (color_info == 1) {
		    ply_write(ply_fp, all_colors[0]);
		    ply_write(ply_fp, all_colors[1]);
		    ply_write(ply_fp, all_colors[2]);
		}
		bu_free(f_regs[ri][fi], "v_ind");
	    }
	    bu_free(f_regs[ri], "reg_faces");
	    bu_hash_destroy(v_tbl_regs[ri]);
	}
	ply_close(ply_fp);
    }
    if (color_info == 1 && verbose)
	bu_log("Wrote color (%d, %d, %d), since all objects had the same color.\n", all_colors[0], all_colors[1], all_colors[2]);
    else if (color_info == 2 && verbose)
	bu_log("No color information specified in BRL-CAD file.\n");
    else if (color_info == 3 && verbose)
	bu_log("All objects not the same color. No color information written.\n");

    if (regions_tried>0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
	       regions_tried, regions_converted, percent);
    }

    if (regions_tried > 0) {
	percent = ((double)regions_written * 100) / regions_tried;
	bu_log("                  %d triangulated successfully. %g%%\n",
	       regions_written, percent);
    }

    bu_log("%zd triangles written\n", tot_polygons);

    bu_log("Tessellation parameters used:\n");
    bu_log("  abs  [-a]    %g\n", ttol.abs);
    bu_log("  rel  [-r]    %g\n", ttol.rel);
    bu_log("  norm [-n]    %g\n", ttol.norm);
    bu_log("  dist [-D]    %g\n", tol.dist);

free_all:
    /* Release dynamic storage */
    if (f_regs)
	bu_free(f_regs, "f_regs");
    if (f_sizes)
	bu_free(f_sizes, "f_sizes");
    if (v_regs)
	bu_free(v_regs, "v_regs");
    if (v_tbl_regs)
	bu_free(v_tbl_regs, "v_tbl_regs");
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

    return 0;
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
