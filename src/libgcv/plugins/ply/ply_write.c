/*                  P L Y _ W R I T E . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2026 United States Government as represented by
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
#include "bu/color.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "raytrace.h"
#include "rplyfile.h"

struct ply_write_options
{
    double rel;                                /* relative tessolation tolerance */
    double abs;                                /* absolute tessolation tolerance */
    double norm;                               /* surface normal tessolation tolerance */
    double dist;                               /* distance calculation tolerance */
    char* o_file;                              /* output file name */
    const char *type;                           /* storage type */
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

    struct bu_list *vlfree;

    // globals from g-ply
    int (*region_colors)[3];
    size_t colored_regions;
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
    int failed;
    e_ply_storage_mode storage_mode;
};


static void
ply_output_error(p_ply output, const char *message)
{
    struct conversion_state *state = NULL;
    if (output && ply_get_ply_user_data(output, (void **)&state, NULL) && state)
	state->failed = 1;
    bu_log("PLY writer: %s\n", message);
}

static e_ply_storage_mode
ply_output_storage_mode(const char *type, const struct bu_mime_traits *requested)
{
    e_ply_storage_mode mode = ply_get_native_storage_mode();
    if (type) {
	if (BU_STR_EQUAL(type, "asc")) mode = PLY_ASCII;
	else if (BU_STR_EQUAL(type, "le")) mode = PLY_LITTLE_ENDIAN;
	else if (BU_STR_EQUAL(type, "be")) mode = PLY_BIG_ENDIAN;
	else if (!BU_STR_EQUAL(type, "de")) {
	    bu_log("PLY writer: unsupported storage type %s\n", type);
	    return PLY_INVALID_STORAGE;
	}
    } else {
	const char *encoding = bu_mime_traits_get(requested, "encoding");
	const char *byte_order = bu_mime_traits_get(requested, "byte-order");
	if (encoding && BU_STR_EQUAL(encoding, "ascii")) mode = PLY_ASCII;
	else if (byte_order && BU_STR_EQUAL(byte_order, "little-endian")) mode = PLY_LITTLE_ENDIAN;
	else if (byte_order && BU_STR_EQUAL(byte_order, "big-endian")) mode = PLY_BIG_ENDIAN;
    }
    const struct bu_mime_trait values[] = {
	BU_MIME_TRAIT_STRING("version", "1.0"),
	BU_MIME_TRAIT_STRING("encoding", mode == PLY_ASCII ? "ascii" : "binary"),
	BU_MIME_TRAIT_STRING("byte-order", mode == PLY_BIG_ENDIAN ? "big-endian" : "little-endian")
    };
    size_t count = sizeof(values) / sizeof(values[0]);
    if (mode == PLY_ASCII) --count;
    const struct bu_mime_capability profile = BU_MIME_CAPABILITY("application/x-ply", values, count, 0);
    if (!bu_mime_capability_match(&profile, requested, BU_MIME_OUTPUT)) {
	bu_log("PLY writer: storage mode contradicts requested output traits\n");
	return PLY_INVALID_STORAGE;
    }
    return mode;
}

static p_ply
ply_output_open(struct conversion_state *state, const char *filename)
{
    return ply_create(filename, state->storage_mode, ply_output_error, 0, state);
}

static void
ply_region_filename(struct bu_vls *filename, const char *output_file, const char *region_name)
{
    struct bu_vls directory = BU_VLS_INIT_ZERO;
    struct bu_vls basename = BU_VLS_INIT_ZERO;
    struct bu_vls region = BU_VLS_INIT_ZERO;
    if (!bu_path_component(&directory, output_file, BU_PATH_DIRNAME))
	bu_vls_strcpy(&directory, ".");
    bu_path_component(&basename, output_file, BU_PATH_BASENAME);
    bu_vls_strcpy(&region, region_name[0] == '/' ? region_name + 1 : region_name);
    for (char *p = bu_vls_addr(&region); *p; ++p)
	if (*p == '/' || *p == '\\')
	    *p = '_';
    bu_vls_printf(filename, "%s/%s_%s", bu_vls_cstr(&directory), bu_vls_cstr(&region), bu_vls_cstr(&basename));
    bu_vls_free(&directory);
    bu_vls_free(&basename);
    bu_vls_free(&region);
}

static p_ply
ply_output_region_open(struct conversion_state *state, const char *region_name)
{
    struct bu_vls filename = BU_VLS_INIT_ZERO;
    struct bu_vls comment = BU_VLS_INIT_ZERO;
    ply_region_filename(&filename, state->output_file, region_name);
    p_ply output = ply_output_open(state, bu_vls_cstr(&filename));
    bu_vls_printf(&comment, "wrote region %s", region_name);
    if (output && (!ply_add_comment(output, "converted from BRL-CAD") || !ply_add_comment(output, bu_vls_cstr(&comment))))
	state->failed = 1;
    bu_vls_free(&filename);
    bu_vls_free(&comment);
    return output;
}

static void
ply_output_value(p_ply output, double value, struct conversion_state *state)
{
    if (!state->failed && !ply_write(output, value))
	state->failed = 1;
}

static int
ply_output_header(p_ply output, size_t vertices, size_t faces, int colors, struct conversion_state *state)
{
    if (vertices >= INT_MAX || faces >= INT_MAX ||
	!ply_add_element(output, "vertex", (long)vertices) ||
	!ply_add_scalar_property(output, "x", PLY_FLOAT) ||
	!ply_add_scalar_property(output, "y", PLY_FLOAT) ||
	!ply_add_scalar_property(output, "z", PLY_FLOAT) ||
	!ply_add_element(output, "face", (long)faces) ||
	!ply_add_list_property(output, "vertex_indices", PLY_UCHAR, PLY_UINT) ||
	(colors && (!ply_add_scalar_property(output, "red", PLY_UCHAR) ||
	 !ply_add_scalar_property(output, "green", PLY_UCHAR) || !ply_add_scalar_property(output, "blue", PLY_UCHAR))) ||
	!ply_write_header(output)) {
	state->failed = 1;
	return 0;
    }
    return 1;
}

static void
ply_region_free(struct conversion_state *state, size_t region)
{
    if (region >= state->tot_regions)
	return;
    size_t faces = state->f_sizes ? (size_t)state->f_sizes[region] : 0;
    if (state->f_regs && state->f_regs[region]) {
	for (size_t i = 0; i < faces; ++i)
	    bu_free(state->f_regs[region][i], "PLY face indices");
	bu_free(state->f_regs[region], "PLY region faces");
	state->f_regs[region] = NULL;
    }
    if (state->v_regs && state->v_regs[region]) {
	for (size_t i = 0; i < faces * 3; ++i)
	    bu_free(state->v_regs[region][i], "PLY vertex coordinates");
	bu_free(state->v_regs[region], "PLY region vertices");
	state->v_regs[region] = NULL;
    }
    if (state->v_tbl_regs && state->v_tbl_regs[region]) {
	bu_hash_destroy(state->v_tbl_regs[region]);
	state->v_tbl_regs[region] = NULL;
    }
    if (state->f_sizes)
	state->f_sizes[region] = 0;
}


static struct bu_hash_entry *
write_verts(p_ply fp, struct bu_hash_tbl *t, struct conversion_state* pstate)
{
    const double millimeters_per_meter = 1000.0;
    struct bu_hash_entry *v_entry = bu_hash_next(t, NULL);
    while (v_entry) {
	double *coords = (double *)(bu_hash_value(v_entry, NULL));
	if (!coords) return v_entry;

	/* PLY files are usually in meters */
	ply_output_value(fp, coords[0] / millimeters_per_meter, pstate);
	ply_output_value(fp, coords[1] / millimeters_per_meter, pstate);
	ply_output_value(fp, coords[2] / millimeters_per_meter, pstate);

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
static void
nmg_to_ply(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id), struct db_tree_state *tsp, struct conversion_state* pstate)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name = NULL;
    size_t nvertices = 0;
    size_t nfaces = 0;
    int color[3] = {0};
    int reg_faces_pos = 0;
    p_ply ply_fp = NULL;

    if (pstate->failed)
	return;
    if ((size_t)pstate->cur_region >= pstate->tot_regions) {
	pstate->failed = 1;
	return;
    }
    if (tsp->ts_mater.ma_color_valid) {
	struct bu_color region_color = BU_COLOR_INIT_ZERO;
	vect_t rgb;
	VMOVE(rgb, tsp->ts_mater.ma_color);
	if (!bu_color_from_rgb_floats(&region_color, rgb) ||
	    !bu_color_to_rgb_ints(&region_color, &color[0], &color[1], &color[2])) {
	    pstate->failed = 1;
	    return;
	}
	if (!pstate->ply_write_options->separate) {
	    memcpy(pstate->region_colors[pstate->cur_region], color, sizeof(color));
	    ++pstate->colored_regions;
	}
    }
    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    if (pstate->ply_write_options->separate) {
	ply_fp = ply_output_region_open(pstate, region_name);
	if (!ply_fp || pstate->failed)
	    goto free_nmg;
    }

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, pstate->vlfree, &pstate->gcv_options->calculational_tolerance);

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
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;
		if ((size_t)reg_faces_pos >= nfaces)
		    goto free_nmg;
		pstate->f_regs[pstate->cur_region][reg_faces_pos] = (long *)bu_calloc(3, sizeof(long), "v_ind");

		if (pstate->ply_write_options->verbose || pstate->gcv_options->verbosity_level)
		    bu_log("\tfacet:\n");
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    if (v_ind_pos >= 3)
			goto free_nmg;
		    pstate->f_regs[pstate->cur_region][reg_faces_pos][v_ind_pos] = v->index;
		    if (pstate->ply_write_options->verbose || pstate->gcv_options->verbosity_level)
			bu_log("\t\t(%g %g %g)\n", V3ARGS(v->vg_p->coord));

		    if (!bu_hash_get(pstate->v_tbl_regs[pstate->cur_region], (const uint8_t *)(pstate->f_regs[pstate->cur_region][reg_faces_pos] + v_ind_pos), sizeof(long))) {
			pstate->v_regs[pstate->cur_region][nvertices] = (double *) bu_calloc(4, sizeof(double), "v_coords");
			pstate->v_regs[pstate->cur_region][nvertices][0] = (double)(v->vg_p->coord[0]);
			pstate->v_regs[pstate->cur_region][nvertices][1] = (double)(v->vg_p->coord[1]);
			pstate->v_regs[pstate->cur_region][nvertices][2] = (double)(v->vg_p->coord[2]);
			(void)bu_hash_set(pstate->v_tbl_regs[pstate->cur_region], (const uint8_t *)(pstate->f_regs[pstate->cur_region][reg_faces_pos] + v_ind_pos), sizeof(long), (void *)pstate->v_regs[pstate->cur_region][nvertices]);
			nvertices++;
		    }

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
	bu_log("ERROR: Number of vertices (%zu) exceeds integer limit!\n", nvertices);
	goto free_nmg;
    }

    if (pstate->ply_write_options->separate) {
	size_t fi;
	size_t vi;
	double *coords;

	if (!ply_output_header(ply_fp, nvertices, nfaces, tsp->ts_mater.ma_color_valid, pstate))
	    goto free_nmg;

	if (write_verts(ply_fp, pstate->v_tbl_regs[pstate->cur_region], pstate)) {
	    bu_log("ERROR: No coordinates found for vertex!\n");
	    goto free_nmg;
	}
	for (fi = 0; fi < nfaces; fi++) {
	    ply_output_value(ply_fp, 3, pstate);
	    for (vi = 0; vi < 3; vi++) {
		coords = (double *)bu_hash_get(pstate->v_tbl_regs[pstate->cur_region], (const unsigned char *)(pstate->f_regs[pstate->cur_region][fi] + vi), sizeof(long));
		if (!coords) {
		    bu_log("ERROR: No vertex found for face with index %ld!\n", pstate->f_regs[pstate->cur_region][fi][vi]);
		    goto free_nmg;
		}
		ply_output_value(ply_fp, coords[3], pstate);
	    }
	    if(tsp->ts_mater.ma_color_valid) {
		ply_output_value(ply_fp, color[0], pstate);
		ply_output_value(ply_fp, color[1], pstate);
		ply_output_value(ply_fp, color[2], pstate);
	    }

	}

        pstate->v_order = 0;    // reset index for next file
        if (!ply_close(ply_fp))
	    pstate->failed = 1;
	ply_fp = NULL;
    }

    pstate->tot_polygons += nfaces;
    pstate->tot_vertices += nvertices;

    if (pstate->ply_write_options->separate)
	ply_region_free(pstate, (size_t)pstate->cur_region);
    bu_free(region_name, "region_name");
    pstate->cur_region++;
    return;

free_nmg:
    pstate->failed = 1;
    if (ply_fp)
	ply_close(ply_fp);
    bu_free(region_name, "region_name");
    ply_region_free(pstate, (size_t)pstate->cur_region);
}


static int
process_triangulation(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp, struct conversion_state* pstate)
{
    volatile int completed = 0;
    if (!BU_SETJUMP) {
	/* try */

	/* Write the facetized region to the output file */
	nmg_to_ply(r, pathp, tsp->ts_regionid, tsp->ts_gmater, tsp, pstate);
	completed = 1;
    } else {
	pstate->failed = 1;
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
    return completed;
}


static union tree *
process_boolean(union tree **curtree, struct db_tree_state *tsp, const struct db_full_path *pathp, struct bu_list *vlfree, struct conversion_state *state)
{
    union tree *volatile ret_tree = TREE_NULL;

    /* Begin bomb protection */
    if (!BU_SETJUMP) {
	/* try */

	(void)nmg_model_fuse(*tsp->ts_m, vlfree, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(*curtree, vlfree, tsp->ts_tol);
    } else {
	/* catch */
	char *name = db_path_to_string(pathp);

	/* Error, bail out */
	bu_log("conversion of %s FAILED!\n", name);

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Release the tree memory & input regions */
	db_free_tree(*curtree);/* Does an nmg_kr() */
	*curtree = TREE_NULL;
	ret_tree = TREE_NULL;
	state->failed = 1;

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
do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    struct conversion_state* pstate = (struct conversion_state*)client_data;

    union tree *ret_tree;
    struct bu_list vhead;
    struct nmgregion *r;
    struct model *result_model = NULL;
    int shared_model = 0;

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

    if (pstate->ply_write_options->verbose || pstate->gcv_options->verbosity_level) {
	char *name = db_path_to_string(pathp);
	bu_log("Attempting to process region %s\n", name);
	bu_free(name, "region path");
    }

    ret_tree = process_boolean(&curtree, tsp, pathp, pstate->vlfree, pstate);
    if (!curtree) {
	BU_ALLOC(curtree, union tree);
	RT_TREE_INIT(curtree);
	curtree->tr_op = OP_NOP;
	return curtree;
    }

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
	/* Tessellated leaves own models independent of the tree-state model. */
	result_model = r->m_p;
	shared_model = result_model == *tsp->ts_m;
	struct db_tree_state region_state = *tsp;
	region_state.ts_m = &result_model;
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
	    empty_model = nmg_kill_zero_length_edgeuses(result_model);
	}

	if (!empty_region && !empty_model) {
	    /* Zero-length edge cleanup can remove the region even when other
	     * regions keep the model alive. Never reuse a removed region. */
	    int present = 0;
	    struct nmgregion *candidate;
	    for (BU_LIST_FOR(candidate, nmgregion, &result_model->r_hd))
		if (candidate == r) present = 1;
	    if (!present || !process_triangulation(r, pathp, &region_state, pstate))
		ret_tree->tr_d.td_r = NULL;
	    else if (!pstate->failed)
		pstate->regions_written++;
	} else if (empty_model) {
	    ret_tree->tr_d.td_r = NULL;
	}
    }

    /* The tree owns the result region, but its tessellation model must be
     * released separately. Return an empty node so the walk retains neither.
     */
    db_free_tree(curtree);
    if (shared_model)
	*tsp->ts_m = result_model;
    else if (result_model)
	nmg_km(result_model);

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}


static int
count_regions(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), const struct rt_comb_internal *UNUSED(combp), void* client_data)
{
    struct conversion_state * const pstate = (struct conversion_state *)client_data;
    pstate->tot_regions++;
    return -1;
}


/* taken from g-tankill.c */
static union tree *
region_stub(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), union tree *curtree, void *client_data)
{
    struct conversion_state *state = (struct conversion_state *)client_data;
    state->failed = 1;
    bu_log("PLY writer: unexpected region callback while counting regions\n");
    return curtree;
}


/* taken from g-tankill.c */
static union tree *
leaf_stub(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), struct rt_db_internal *UNUSED(ip), void *client_data)
{
    struct conversion_state *state = (struct conversion_state *)client_data;
    state->failed = 1;
    bu_log("PLY writer: unexpected leaf callback while counting regions\n");
    return TREE_NULL;
}

static void
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
    options_data->type = NULL;
    options_data->separate = 0;
    options_data->verbose = 0;
    options_data->x = NULL;
    options_data->X = NULL;

    BU_OPT((*options_desc)[0],  "r", "rel_tess_tol",  "decimal", bu_opt_fastf_t, &options_data->rel,       "specify the relative tessolation tolerance");
    BU_OPT((*options_desc)[1],  "a", "abs_tess_tol",  "decimal", bu_opt_fastf_t, &options_data->abs,       "specify the absolute tessolation tolerance");
    BU_OPT((*options_desc)[2],  "n", "norm_tess_tol", "decimal", bu_opt_fastf_t, &options_data->norm,      "specify the surface normal tessolation tolerance");
    BU_OPT((*options_desc)[3],  "D", "dist_calc_tol", "decimal", bu_opt_fastf_t, &options_data->dist,      "specify the distance calculation tolerance");
    BU_OPT((*options_desc)[4],  "o", "output_file",   "str",   bu_opt_str,     &options_data->o_file,    "specify the output file name");
    BU_OPT((*options_desc)[5],  "t", "type",          "str",   bu_opt_str,     &options_data->type,      "specify output storage (asc: ASCII), (le: little endian), (be: big endian), (de: native binary)");
    BU_OPT((*options_desc)[6],  "s", "separate",      "",      NULL,           &options_data->separate, "specify a flag to generate a separate file per object");
    BU_OPT((*options_desc)[7],  "v", "verbose",       "",      NULL,           &options_data->verbose,   "specify a flag for verbose output");
    BU_OPT((*options_desc)[8],  "x", "rt_debug",      "str",   bu_opt_str,     &options_data->x,         "specify debug script for rt_debug");
    BU_OPT((*options_desc)[9],  "X", "nmg_debug",     "str",   bu_opt_str,     &options_data->X,         "specify debug script for nmg_debug");
    BU_OPT_NULL((*options_desc)[10]);
}

static void
ply_write_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

static int
ply_write_gcv(struct gcv_context* context, const struct gcv_opts* gcv_options, const void* options_data, const char* dest_path)
{
    struct conversion_state state;
    struct db_tree_state tree_state;
    p_ply ply_fp = NULL;
    int status = 0;

    if (!gcv_options->num_objects) {
	bu_log("PLY writer: no objects to export\n");
	return 0;
    }
    for (size_t i = 0; i < gcv_options->num_objects; ++i)
	if (db_lookup(context->dbip, gcv_options->object_names[i], LOOKUP_NOISY) == RT_DIR_NULL)
	    return 0;

    memset(&state, 0, sizeof(state));
    state.gcv_options = gcv_options;
    struct ply_write_options output_options = *(const struct ply_write_options *)options_data;
    /* An explicit legacy mode can choose another compatible variant;
     * defaults from the first MIME clause are not caller assertions. */
    const struct bu_mime_traits *requested = output_options.type ?
	gcv_target_request_traits(context) : gcv_target_traits(context);
    state.ply_write_options = &output_options;
    state.storage_mode = ply_output_storage_mode(output_options.type, requested);
    if (state.storage_mode == PLY_INVALID_STORAGE)
	return 0;
    state.output_file = dest_path;
    if (state.ply_write_options->o_file) {        // check if user explicitly set with -o
        state.output_file = state.ply_write_options->o_file;
    }
    state.dbip = context->dbip;

    RT_DBTS_INIT(&tree_state);
    struct bn_tol tol = state.gcv_options->calculational_tolerance;             // init tol with gcv_options defaults
    struct bg_tess_tol ttol = state.gcv_options->tessellation_tolerance;        // init ttol with gcv_options defaults
    tree_state.ts_tol = &tol;   // point tree_state at modifiable tol
    tree_state.ts_ttol = &ttol; // point tree_state at modifiable ttol
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

    state.vlfree = &rt_vlfree;

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

    if (!state.ply_write_options->separate)
	state.region_colors = (int (*)[3])bu_calloc(state.tot_regions, sizeof(*state.region_colors), "PLY region colors");
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
			rt_booltree_leaf_tess,
			(void*)&state);

    if (state.ply_write_options->verbose || state.gcv_options->verbosity_level)
	bu_log("Found %d nmg regions.\n", state.cur_region);

    if (state.failed || !state.cur_region)
	goto free_all;

    if (!state.ply_write_options->separate && state.colored_regions &&
	state.colored_regions < (size_t)state.cur_region &&
	!gcv_report_loss(context, GCV_LOSS_APPEARANCE, "ply.region-colors", state.colored_regions,
	    "Region colors are omitted when merged output includes uncolored regions"))
	goto free_all;

    if (!state.ply_write_options->separate) {
	const int output_colors = state.colored_regions == (size_t)state.cur_region;
	int ri;
        int fi;
        int vi;
	double *coords;

        ply_fp = ply_output_open(&state, state.output_file);

        /* verify open and writability */
        if (!ply_fp) {
            bu_log("ERROR: Unable to create PLY file\n");
            goto free_all;
        }
        if (!ply_add_comment(ply_fp, "converted from BRL-CAD")) {
            bu_log("ERROR: Unable to write to PLY file\n");
            goto free_all;
        }

        if (!ply_output_header(ply_fp, state.tot_vertices, state.tot_polygons, output_colors, &state))
	    goto free_all;

	for (ri = 0; ri < state.cur_region; ri++)
	    if (write_verts(ply_fp, state.v_tbl_regs[ri], &state)) {
		bu_log("ERROR: No coordinates found for vertex!\n");
		goto free_all;
	    }
	for (ri = 0; ri < state.cur_region; ri++) {
	    for (fi = 0; fi < state.f_sizes[ri]; fi++) {
		ply_output_value(ply_fp, 3, &state);
		for (vi = 0; vi < 3; vi++) {
		    coords = (double *)bu_hash_get(state.v_tbl_regs[ri], (const unsigned char *)(state.f_regs[ri][fi] + vi), sizeof(long));
		    if (!coords) {
			bu_log("ERROR: No vertex found for face with index %ld!\n", state.f_regs[ri][fi][vi]);
			goto free_all;
		    }
		    ply_output_value(ply_fp, coords[3], &state);
		}
		if (output_colors) {
		    ply_output_value(ply_fp, state.region_colors[ri][0], &state);
		    ply_output_value(ply_fp, state.region_colors[ri][1], &state);
		    ply_output_value(ply_fp, state.region_colors[ri][2], &state);
		}
	    }
	    ply_region_free(&state, (size_t)ri);
	}
    }

    if (state.failed)
	goto free_all;

    if (!state.ply_write_options->separate && state.colored_regions == (size_t)state.cur_region &&
	(state.ply_write_options->verbose || state.gcv_options->verbosity_level))
	bu_log("Wrote region colors as PLY face colors.\n");

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

    status = !state.failed && state.cur_region > 0;
free_all:
    for (size_t i = 0; i < state.tot_regions; ++i)
	ply_region_free(&state, i);
    /* Release dynamic storage */
    bu_free(state.region_colors, "PLY region colors");
    if (state.f_regs)
	bu_free(state.f_regs, "state.f_regs");
    if (state.f_sizes)
	bu_free(state.f_sizes, "state.f_sizes");
    if (state.v_regs)
	bu_free(state.v_regs, "state.v_regs");
    if (state.v_tbl_regs)
	bu_free(state.v_tbl_regs, "state.v_tbl_regs");
    if (ply_fp && !ply_close(ply_fp))
	status = 0;
    nmg_km(state.the_model);
    rt_vlist_cleanup();

    return status && !state.failed;
}

/* filter setup */
const struct gcv_filter gcv_conv_ply_write = {
    "PLY Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_PLY, NULL,
    ply_write_create_opts, ply_write_free_opts, ply_write_gcv
};

const struct gcv_conversion gcv_conv_ply_write_execution = {"PLY Writer", GCV_FILTER_WRITE, ply_write_create_opts, ply_write_free_opts, ply_write_gcv};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
