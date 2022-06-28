/*                  P L Y _ W R I T E . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2022 United States Government as represented by
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
/** @file libgcv/plugins/ply/ply_write.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to the Stanford
 * PLY format by calling on the NMG booleans. Based on g-stl.c.
 *
 */

#include "common.h"

#include <string.h>

#include "gcv.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rply.h"

struct ply_write_options
{
    double rel;                                /* relative tessolation tolerance */
    double abs;                                /* absolute tessolation tolerance */
    double norm;                               /* surface normal tessolation tolerance */
    double dist;                               /* distance calculation tolerance */
    char* o_file;                              /* output file name */
    char* type;                                /* storage type */
    int separate;                              /* flag to not merge all objects into one ply file */
    int verbose;                               /* flag for verbose output */
    char* x;                                   /* rt_debug */
    char* X;                                   /* nmg_debug */
};

struct conversion_state
{
    const struct gcv_opts *gcv_options;
    const struct ply_write_options *ply_write_options;

    const char *output_file;	               /* output filename */

    struct db_i *dbip;
    FILE *fp;			               /* Output file pointer */
    struct model *the_model;

    // globals from g-ply
    int color_info;
    int all_colors[3];
    int v_order;
    long ***f_regs;
    double ***v_regs;
    struct bu_hash_tbl **v_tbl_regs;
    int *f_sizes;
    int regions_tried;
    int regions_converted;
    int regions_written;
    int cur_region;
    size_t tot_regions;
    size_t tot_polygons;
    size_t tot_vertices;
};


HIDDEN struct bu_hash_entry *
write_verts(p_ply fp, struct bu_hash_tbl *t, struct conversion_state* pstate)
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
	coords[3] = pstate->v_order;
	pstate->v_order++;

	v_entry = bu_hash_next(t, v_entry);
    }
    return NULL;
}


/* routine to output the faceted NMG representation of a BRL-CAD
 * region
 */
HIDDEN void
nmg_to_ply(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id), struct db_tree_state *tsp, struct conversion_state* pstate)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name = NULL;
    size_t nvertices = 0;
    size_t nfaces = 0;
    int color[3];
    int reg_faces_pos = 0;
    p_ply ply_fp = NULL;

    VSCALE(color, tsp->ts_mater.ma_color, 255);
    if (!pstate->ply_write_options->separate) {
	if (!tsp->ts_mater.ma_color_valid) {
	    pstate->color_info = 2;
	} else if (pstate->color_info == 0) {
	    pstate->all_colors[0] = color[0];
	    pstate->all_colors[1] = color[1];
	    pstate->all_colors[2] = color[2];
	    pstate->color_info = 1;
	} else if (pstate->color_info == 1 && (pstate->all_colors[0] != color [0] || pstate->all_colors[1] != color [1] || pstate->all_colors[2] != color [2])) {
	    pstate->color_info = 3;
	}
    }
    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    if (pstate->ply_write_options->separate) {
	/* Open output file */
	char *comment;
	unsigned int k;
	char *region_file;

	region_file = (char *) bu_calloc(strlen(region_name) + strlen(pstate->output_file), sizeof(char), "region_file");
	sprintf(region_file, "%s_%s", region_name+1, pstate->output_file);
	for (k = 0; k < strlen(region_file); k++) {
	    switch (region_file[k]) {
		case '/':
		    region_file[k] = '_';
		    break;
	    }
	}

	/* open outputfile using specified type */
        if (!strcmp(pstate->ply_write_options->type, "asc")) {
            ply_fp = ply_create(region_file, PLY_ASCII, NULL, 0, NULL);
        } else if (!strcmp(pstate->ply_write_options->type, "le")) {
	    ply_fp = ply_create(region_file, PLY_LITTLE_ENDIAN, NULL, 0, NULL);
	} else if (!strcmp(pstate->ply_write_options->type, "be")) {
	    ply_fp = ply_create(region_file, PLY_BIG_ENDIAN, NULL, 0, NULL);
	} else {
	    ply_fp = ply_create(region_file, PLY_DEFAULT, NULL, 0, NULL);
	}
	if (!ply_fp) {
	    bu_hash_destroy(pstate->v_tbl_regs[pstate->cur_region]);
	    bu_free(region_file, "region_file");
	    bu_log("ERROR: Unable to create PLY file");
	    goto free_nmg;
	}
	if (!ply_add_comment(ply_fp, "converted from BRL-CAD")) {
	    bu_hash_destroy(pstate->v_tbl_regs[pstate->cur_region]);
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
    nmg_triangulate_model(m, &RTG.rtg_vlfree, &pstate->gcv_options->calculational_tolerance);

    /* Output triangles */
    if (pstate->ply_write_options->verbose || pstate->gcv_options->verbosity_level)
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
	bu_hash_destroy(pstate->v_tbl_regs[pstate->cur_region]);
	bu_log("ERROR: Number of faces (%zu) exceeds integer limit!\n", nfaces);
	goto free_nmg;
    }

    pstate->f_sizes[pstate->cur_region] = (int) nfaces;
    pstate->v_tbl_regs[pstate->cur_region] = bu_hash_create(nfaces * 3);
    pstate->f_regs[pstate->cur_region] = (long **) bu_calloc(nfaces, sizeof(long *), "reg_faces");
    pstate->v_regs[pstate->cur_region] = (double **) bu_calloc(nfaces * 3, sizeof(double *), "reg_verts"); /* chose to do this over dynamic array, but I may be wrong */

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
		pstate->f_regs[pstate->cur_region][reg_faces_pos] = (long *) bu_calloc(3, sizeof(long), "v_ind");
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		if (pstate->ply_write_options->verbose || pstate->gcv_options->verbosity_level)
		    bu_log("\tfacet:\n");
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    pstate->f_regs[pstate->cur_region][reg_faces_pos][v_ind_pos] = v->index;
		    if (pstate->ply_write_options->verbose || pstate->gcv_options->verbosity_level)
			bu_log("\t\t(%g %g %g)\n", V3ARGS(v->vg_p->coord));

		    if (!bu_hash_get(pstate->v_tbl_regs[pstate->cur_region], (const uint8_t *)(pstate->f_regs[pstate->cur_region][reg_faces_pos] + v_ind_pos), sizeof(long *))) {
			pstate->v_regs[pstate->cur_region][nvertices] = (double *) bu_calloc(4, sizeof(double), "v_coords");
			pstate->v_regs[pstate->cur_region][nvertices][0] = (double)(v->vg_p->coord[0]);
			pstate->v_regs[pstate->cur_region][nvertices][1] = (double)(v->vg_p->coord[1]);
			pstate->v_regs[pstate->cur_region][nvertices][2] = (double)(v->vg_p->coord[2]);
			(void)bu_hash_set(pstate->v_tbl_regs[pstate->cur_region], (const uint8_t *)(pstate->f_regs[pstate->cur_region][reg_faces_pos] + v_ind_pos), sizeof(long *), (void *)pstate->v_regs[pstate->cur_region][nvertices]);
			nvertices++;
		    }

		    if (v_ind_pos > 2) {
			bu_log("ERROR: Too many vertices per face!\n");
			goto free_nmg;
		    }
		    pstate->f_regs[pstate->cur_region][reg_faces_pos][v_ind_pos] = v->index;
		    v_ind_pos++;
		}
		if ((size_t)reg_faces_pos >= nfaces) {
		    bu_log("ERROR: More loopuses than faces!\n");
		    goto free_nmg;
		}
		reg_faces_pos++;
	    }
	}
    }

    if (nvertices >= INT_MAX) {
	bu_hash_destroy(pstate->v_tbl_regs[pstate->cur_region]);
	bu_log("ERROR: Number of vertices (%zu) exceeds integer limit!\n", nvertices);
	goto free_nmg;
    }

    if (pstate->ply_write_options->separate) {
	size_t fi;
	size_t vi;
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

	if (write_verts(ply_fp, pstate->v_tbl_regs[pstate->cur_region], pstate)) {
	    bu_hash_destroy(pstate->v_tbl_regs[pstate->cur_region]);
	    bu_log("ERROR: No coordinates found for vertex!\n");
	    goto free_nmg;
	}
	for (fi = 0; fi < nfaces; fi++) {
	    ply_write(ply_fp, 3);
	    for (vi = 0; vi < 3; vi++) {
		coords = (double *)bu_hash_get(pstate->v_tbl_regs[pstate->cur_region], (const unsigned char *)(pstate->f_regs[pstate->cur_region][fi] + vi), sizeof(long));
		if (!coords) {
		    bu_log("ERROR: No vertex found for face with index %ld!\n", pstate->f_regs[pstate->cur_region][fi][vi]);
		    goto free_nmg;
		}
		ply_write(ply_fp, coords[3]);
	    }
	    if(tsp->ts_mater.ma_color_valid) {
		ply_write(ply_fp, color[0]);
		ply_write(ply_fp, color[1]);
		ply_write(ply_fp, color[2]);
	    }
	    bu_free(pstate->f_regs[pstate->cur_region][fi], "v_ind");
	}

        pstate->v_order = 0;    // reset index for next file
    }

    pstate->tot_polygons += nfaces;
    pstate->tot_vertices += nvertices;

    if (pstate->ply_write_options->separate) {
	bu_hash_destroy(pstate->v_tbl_regs[pstate->cur_region]);
	bu_free(pstate->f_regs[pstate->cur_region], "reg_faces");
    }
    bu_free(region_name, "region_name");
    pstate->cur_region++;
    return;

free_nmg:
    if (ply_fp)
	ply_close(ply_fp);
    if (region_name)
	bu_free(region_name, "region_name");
    if (pstate->f_regs)
	bu_free(pstate->f_regs, "pstate->f_regs");
    if (pstate->f_sizes)
	bu_free(pstate->f_sizes, "pstate->f_sizes");
    if (pstate->v_regs)
	bu_free(pstate->v_regs, "pstate->v_regs");
    if (pstate->v_tbl_regs)
	bu_free(pstate->v_tbl_regs, "pstate->v_tbl_regs");
    nmg_km(pstate->the_model);
    rt_vlist_cleanup();
    bu_exit(1, NULL);
}


HIDDEN void
process_triangulation(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp, struct conversion_state* pstate)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the facetized region to the output file */
	nmg_to_ply(r, pathp, tsp->ts_regionid, tsp->ts_gmater, tsp, pstate);
    } else {
	/* catch */

	char *sofar;

	sofar = db_path_to_string(pathp);
	bu_log("FAILED in triangulator: %s\n", sofar);
	bu_free((char *)sofar, "sofar");

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


HIDDEN union tree *
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
HIDDEN union tree *
do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    struct conversion_state* pstate = (struct conversion_state*)client_data;

    union tree *ret_tree;
    struct bu_list vhead;
    struct nmgregion *r;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    if (pstate->ply_write_options->verbose || pstate->gcv_options->verbosity_level) {
	char *sofar = db_path_to_string(pathp);
	bu_log("\ndo_region_end(%d %d%%) %s\n",
	       pstate->regions_tried,
	       pstate->regions_tried>0 ? (pstate->regions_converted * 100) / pstate->regions_tried : 100,
	       sofar);
	bu_free(sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP)
	return curtree;

    pstate->regions_tried++;

    if (pstate->ply_write_options->verbose || pstate->gcv_options->verbosity_level)
	bu_log("Attempting to process region %s\n", db_path_to_string(pathp));

    ret_tree = process_boolean(curtree, tsp, pathp);

    if (ret_tree) {
	r = ret_tree->tr_d.td_r;
    } else {
	if (pstate->ply_write_options->verbose || pstate->gcv_options->verbosity_level)
	    bu_log("\tNothing left of this region after Boolean evaluation\n");
	pstate->regions_written++; /* don't count as a failure */
	r = (struct nmgregion *)NULL;
    }

    pstate->regions_converted++;

    if (r != (struct nmgregion *)NULL) {
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
	    process_triangulation(r, pathp, tsp, pstate);

	    pstate->regions_written++;
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


HIDDEN int
count_regions(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), const struct rt_comb_internal *UNUSED(combp), void* client_data)
{
    struct conversion_state * const pstate = (struct conversion_state *)client_data;
    pstate->tot_regions++;
    return -1;
}


/* taken from g-tankill.c */
HIDDEN union tree *
region_stub(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), union tree *UNUSED(curtree), void *UNUSED(client_data))
{
    bu_exit(1, "ERROR; region stub called, this shouldn't happen.\n");
    return (union tree *)NULL; /* just to keep the compilers happy */
}


/* taken from g-tankill.c */
HIDDEN union tree *
leaf_stub(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), struct rt_db_internal *UNUSED(ip), void *UNUSED(client_data))
{
    bu_exit(1, "ERROR: leaf stub called, this shouldn't happen.\n");
    return (union tree *)NULL; /* just to keep the compilers happy */
}

HIDDEN void
ply_write_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct ply_write_options *options_data;

    BU_ALLOC(options_data, struct ply_write_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(11 * sizeof(struct bu_opt_desc), "options_desc");

    /* most options defaults are handled by gcv_opts */
    options_data->rel = -1;
    options_data->abs = -1;
    options_data->norm = -1;
    options_data->dist = -1;
    options_data->o_file = NULL;
    options_data->type = "de";
    options_data->separate = 0;
    options_data->verbose = 0;
    options_data->x = NULL;
    options_data->X = NULL;

    BU_OPT((*options_desc)[0],  "r", "rel_tess_tol",  "decimal", bu_opt_fastf_t, &options_data->rel,       "specify the relative tessolation tolerance");
    BU_OPT((*options_desc)[1],  "a", "abs_tess_tol",  "decimal", bu_opt_fastf_t, &options_data->abs,       "specify the absolute tessolation tolerance");
    BU_OPT((*options_desc)[2],  "n", "norm_tess_tol", "decimal", bu_opt_fastf_t, &options_data->norm,      "specify the surface normal tessolation tolerance");
    BU_OPT((*options_desc)[3],  "D", "dist_calc_tol", "decimal", bu_opt_fastf_t, &options_data->dist,      "specify the distance calculation tolerance");
    BU_OPT((*options_desc)[4],  "o", "output_file",   "str",   bu_opt_str,     &options_data->o_file,    "specify the output file name");
    BU_OPT((*options_desc)[5],  "t", "type",          "str",   bu_opt_str,     &options_data->type,      "specify the output file type (asc: ascii), (le: little endian), (be: big endian)");
    BU_OPT((*options_desc)[6],  "s", "separate",      "",      NULL,           &options_data->separate, "specify a flag to generate a separate file per object");
    BU_OPT((*options_desc)[7],  "v", "verbose",       "",      NULL,           &options_data->verbose,   "specify a flag for verbose output");
    BU_OPT((*options_desc)[8],  "x", "rt_dubug",      "str",   bu_opt_str,     &options_data->o_file,    "specify debug script for rt_debug");
    BU_OPT((*options_desc)[9],  "X", "nmg_debug",     "str",   bu_opt_str,     &options_data->o_file,    "specify debug script for nmg_debug");
    BU_OPT_NULL((*options_desc)[10]);
}

HIDDEN void
ply_write_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

HIDDEN int
ply_write_gcv(struct gcv_context* context, const struct gcv_opts* gcv_options, const void* options_data, const char* dest_path)
{
    struct conversion_state state;
    struct db_tree_state tree_state;
    p_ply ply_fp = NULL;

    memset(&state, 0, sizeof(state));
    state.gcv_options = gcv_options;
    state.ply_write_options = (struct ply_write_options*)options_data;
    state.output_file = dest_path;
    if (state.ply_write_options->o_file) {        // check if user explicitly set with -o
        state.output_file = state.ply_write_options->o_file;
    }
    state.dbip = context->dbip;
    state.color_info = 0;

    tree_state = rt_initial_tree_state; /* struct copy */
    struct bn_tol tol = state.gcv_options->calculational_tolerance;             // init tol with gcv_options defaults
    struct bg_tess_tol ttol = state.gcv_options->tessellation_tolerance;        // init ttol with gcv_options defaults
    tree_state.ts_tol = &tol;   // point tree_state at modifable tol
    tree_state.ts_ttol = &ttol; // point tree_state at modifable ttol
    tree_state.ts_m = &state.the_model;

    /* update tolerances if user explicitly set with flag */
    if (state.ply_write_options->abs >= 0)
        ttol.abs = state.ply_write_options->abs;
    if (state.ply_write_options->rel >= 0)
        ttol.rel = state.ply_write_options->rel;
    if (state.ply_write_options->norm >= 0)
        ttol.norm = state.ply_write_options->norm;
    if (state.ply_write_options->dist >= 0) {
        tol.dist = state.ply_write_options->dist;
        tol.dist_sq = state.ply_write_options->dist * state.ply_write_options->dist;
    }

    /* make empty NMG model */
    state.the_model = nmg_mm();

    BU_LIST_INIT(&RTG.rtg_vlfree);      /* for vlist macros */

    if (!state.ply_write_options->separate) {
        /* open outputfile using specified type */
        if (!strcmp(state.ply_write_options->type, "asc")) {
            ply_fp = ply_create(state.output_file, PLY_ASCII, NULL, 0, NULL);
        } else if (!strcmp(state.ply_write_options->type, "le")) {
	    ply_fp = ply_create(state.output_file, PLY_LITTLE_ENDIAN, NULL, 0, NULL);
	} else if (!strcmp(state.ply_write_options->type, "be")) {
	    ply_fp = ply_create(state.output_file, PLY_BIG_ENDIAN, NULL, 0, NULL);
	} else {
	    ply_fp = ply_create(state.output_file, PLY_DEFAULT, NULL, 0, NULL);
	}

        /* verify open and writability */
        if (!ply_fp) {
            bu_log("ERROR: Unable to create PLY file\n");
            goto free_all;
        }
        if (!ply_add_comment(ply_fp, "converted from BRL-CAD")) {
            bu_log("ERROR: Unable to write to PLY file\n");
            goto free_all;
        }

        state.all_colors[0] = -1;
        state.all_colors[1] = -1;
        state.all_colors[2] = -1;
    }

    if (state.ply_write_options->verbose || state.gcv_options->verbosity_level) {
	bu_log("Model: %s\n", gcv_options->object_names[0]);
	bu_log("Objects:");
	for (size_t i = 0; i < gcv_options->num_objects; i++)
	    bu_log(" %s", gcv_options->object_names[i]);
	bu_log("\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	    tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
	bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	    tree_state.ts_tol->dist, tree_state.ts_tol->perp);
    }

    /* Verify that all the specified objects are valid - if one or
     * more of them are not, bail
     */
    for (size_t j = 0; j < gcv_options->num_objects; j++) {
	if (db_lookup(state.dbip, gcv_options->object_names[j], LOOKUP_NOISY) == RT_DIR_NULL)
	    bu_exit(1, "ERROR: invalid object\n");
    }

    /* Quickly get number of regions. I did this over dynamically
     * allocating the memory, but that may be wrong
     */
    (void) db_walk_tree(state.dbip,
    			gcv_options->num_objects,
			(const char**)gcv_options->object_names,
			1,		/* ncpu */
			&tree_state,
			count_regions,
			region_stub,
			leaf_stub,
			(void*)&state);
    if (state.ply_write_options->verbose || state.gcv_options->verbosity_level)
	bu_log("Found %zu total regions.\n", state.tot_regions);

    state.f_regs = (long ***) bu_calloc(state.tot_regions, sizeof(long**), "state.f_regs");
    state.f_sizes = (int *) bu_calloc(state.tot_regions, sizeof(int), "state.f_sizes");
    state.v_regs = (double ***) bu_calloc(state.tot_regions, sizeof(double**), "state.v_regs");
    state.v_tbl_regs = (struct bu_hash_tbl **) bu_calloc(state.tot_regions, sizeof(struct bu_hash_tbl*), "v_tbl_regs");

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(state.dbip, 
    			gcv_options->num_objects, 
			(const char**)gcv_options->object_names,
			1,		/* ncpu */
			&tree_state,
			0,		/* take all regions */
			do_region_end,
			nmg_booltree_leaf_tess,
			(void*)&state);

    if (state.ply_write_options->verbose || state.gcv_options->verbosity_level)
	bu_log("Found %d nmg regions.\n", state.cur_region);

    if (!state.ply_write_options->separate) {
	int ri;
        int fi;
        int vi;
	double *coords;

        ply_add_element(ply_fp, "vertex", state.tot_vertices);
        ply_add_scalar_property(ply_fp, "x", PLY_FLOAT);
        ply_add_scalar_property(ply_fp, "y", PLY_FLOAT);
        ply_add_scalar_property(ply_fp, "z", PLY_FLOAT);
        ply_add_element(ply_fp, "face", state.tot_polygons);
        ply_add_list_property(ply_fp, "vertex_indices", PLY_UCHAR, PLY_UINT);
        if (state.color_info == 1) {
            ply_add_scalar_property(ply_fp, "red", PLY_UCHAR);
            ply_add_scalar_property(ply_fp, "green", PLY_UCHAR);
            ply_add_scalar_property(ply_fp, "blue", PLY_UCHAR);
        }
        ply_write_header(ply_fp);

	for (ri = 0; ri < state.cur_region; ri++)
	    if (write_verts(ply_fp, state.v_tbl_regs[ri], &state)) {
		bu_log("ERROR: No coordinates found for vertex!\n");
		goto free_all;
	    }
	for (ri = 0; ri < state.cur_region; ri++) {
	    for (fi = 0; fi < state.f_sizes[ri]; fi++) {
		ply_write(ply_fp, 3);
		for (vi = 0; vi < 3; vi++) {
		    coords = (double *)bu_hash_get(state.v_tbl_regs[ri], (const unsigned char *)(state.f_regs[ri][fi] + vi), sizeof(long));
		    if (!coords) {
			bu_log("ERROR: No vertex found for face with index %ld!\n", state.f_regs[ri][fi][vi]);
			goto free_all;
		    }
		    ply_write(ply_fp, coords[3]);
		}
		if (state.color_info == 0) {
		    bu_log("ERROR: Could not find any regions");
		    goto free_all;
		} else if (state.color_info == 1) {
		    ply_write(ply_fp, state.all_colors[0]);
		    ply_write(ply_fp, state.all_colors[1]);
		    ply_write(ply_fp, state.all_colors[2]);
		}
		bu_free(state.f_regs[ri][fi], "v_ind");
	    }
	    bu_free(state.f_regs[ri], "reg_faces");
	    bu_hash_destroy(state.v_tbl_regs[ri]);
	}
    }

    if (state.color_info == 1 && (state.ply_write_options->verbose || state.gcv_options->verbosity_level))
	bu_log("Wrote color (%d, %d, %d), since all objects had the same color.\n", state.all_colors[0], state.all_colors[1], state.all_colors[2]);
    else if (state.color_info == 2 && (state.ply_write_options->verbose || state.gcv_options->verbosity_level))
	bu_log("No color information specified in BRL-CAD file.\n");
    else if (state.color_info == 3 && (state.ply_write_options->verbose || state.gcv_options->verbosity_level))
	bu_log("All objects not the same color. No color information written.\n");

    double percent;
    if (state.regions_tried > 0) {
	percent = ((double)state.regions_converted * 100) / state.regions_tried;
	bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
	       state.regions_tried, state.regions_converted, percent);
    }

    if (state.regions_tried > 0) {
	percent = ((double)state.regions_written * 100) / state.regions_tried;
	bu_log("                  %d triangulated successfully. %g%%\n",
	       state.regions_written, percent);
    }

    bu_log("%zu triangles written\n", state.tot_polygons);

    if (state.ply_write_options->verbose || state.gcv_options->verbosity_level) {
        bu_log("Tessellation parameters used:\n");
        bu_log("  abs  [-a]    %g\n", ttol.abs);
        bu_log("  rel  [-r]    %g\n", ttol.rel);
        bu_log("  norm [-n]    %g\n", ttol.norm);
        bu_log("  dist [-D]    %g\n", tol.dist);
    }

free_all:
    /* Release dynamic storage */
    if (state.f_regs)
	bu_free(state.f_regs, "state.f_regs");
    if (state.f_sizes)
	bu_free(state.f_sizes, "state.f_sizes");
    if (state.v_regs)
	bu_free(state.v_regs, "state.v_regs");
    if (state.v_tbl_regs)
	bu_free(state.v_tbl_regs, "state.v_tbl_regs");
    if (ply_fp)
        ply_close(ply_fp);
    nmg_km(state.the_model);
    rt_vlist_cleanup();

    return 1;
}

/* filter setup */
const struct gcv_filter gcv_conv_ply_write = {
    "PLY Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_PLY, NULL,
    ply_write_create_opts, ply_write_free_opts, ply_write_gcv
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */