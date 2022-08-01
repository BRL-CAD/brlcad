/*                     O B J _ W R I T E . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2022 United States Government as represented by
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
/** @file obj_write.c
 *
 * Convert a BRL-CAD model (in a .g file) to a Wavefront
 * '.obj' file by calling on the NMG booleans.
 *
 */


#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "raytrace.h"
#include "gcv/api.h"

#define V3ARGS_SCALE(v, factor)       (v)[X] * (factor), (v)[Y] * (factor), (v)[Z] * (factor)


struct obj_write_options
{
    int do_normals;
    int usemtl;	         /* flag to include 'usemtl' statements with a
			 * code for GIFT materials:
			 *
			 * pstate->usemtl 0_100_32
			 * means aircode is 0
			 * los is 100
			 * GIFT material is 32
			 */
};


struct conversion_state
{
    const struct gcv_opts *gcv_options;
    const struct obj_write_options *obj_write_options;
    FILE *fp;

    b_off_t vert_offset;
    b_off_t norm_offset;
    size_t regions_tried;
    size_t regions_converted;
    size_t regions_written;
    int nmg_debug;      /* saved for longjmp handling */
};


HIDDEN void
nmg_to_obj(struct conversion_state *pstate, struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int aircode, int los, int material_id)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    struct bu_ptbl verts;
    struct bu_ptbl norms;
    char *region_name;
    size_t numverts = 0;		/* Number of vertices to output */
    size_t numtri   = 0;		/* Number of triangles to output */
    size_t i;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, &RTG.rtg_vlfree, &pstate->gcv_options->calculational_tolerance);

    /* list all vertices in result */
    nmg_vertex_tabulate(&verts, &r->l.magic, &RTG.rtg_vlfree);

    /* Get number of vertices */
    numverts = BU_PTBL_LEN(&verts);

    /* get list of vertexuse normals */
    if (pstate->obj_write_options->do_normals)
	nmg_vertexuse_normal_tabulate(&norms, &r->l.magic, &RTG.rtg_vlfree);

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
			bu_bomb("Can't find vertex in list!");
		    }
		}
		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_bomb("LU is not a triangle\n");
		} else if (vert_count < 3)
		    continue;
		numtri++;
	    }
	}
    }

    /* END CHECK SECTION */
    /* Write pertinent info for this region */

    if (pstate->obj_write_options->usemtl)
	fprintf(pstate->fp, "usemtl %d_%d_%d\n", aircode, los, material_id);

    fprintf(pstate->fp, "g %s", pathp->fp_names[0]->d_namep);
    for (i=1; i<pathp->fp_len; i++)
	fprintf(pstate->fp, "/%s", pathp->fp_names[i]->d_namep);
    fprintf(pstate->fp, "\n");

    /* Write vertices */
    for (i=0; i<numverts; i++) {
	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);

	fprintf(pstate->fp, "v %f %f %f\n", V3ARGS_SCALE(v->vg_p->coord, pstate->gcv_options->scale_factor));
    }

    /* Write vertexuse normals */
    if (pstate->obj_write_options->do_normals) {
	for (i=0; i<BU_PTBL_LEN(&norms); i++) {
	    struct vertexuse_a_plane *va;

	    va = (struct vertexuse_a_plane *)BU_PTBL_GET(&norms, i);
	    NMG_CK_VERTEXUSE_A_PLANE(va);
	    fprintf(pstate->fp, "vn %f %f %f\n", V3ARGS_SCALE(va->N, pstate->gcv_options->scale_factor));
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
		if (pstate->obj_write_options->do_normals) {
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

		fprintf(pstate->fp, "f");

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
			bu_bomb("Can't find vertex in list!\n");
		    }

		    if (use_normals) {
			int j;

			j = bu_ptbl_locate(&norms, (long *)eu->vu_p->a.magic_p);
			fprintf(pstate->fp, " %ld//%ld", loc+1+(long)pstate->vert_offset, j+1+(long)pstate->norm_offset);
		    } else
			fprintf(pstate->fp, " %ld", loc+1+(long)pstate->vert_offset);
		}

		fprintf(pstate->fp, "\n");

		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_bomb("LU is not a triangle\n");
		}
	    }
	}
    }
    pstate->vert_offset += numverts;
    bu_ptbl_free(&verts);
    if (pstate->obj_write_options->do_normals) {
	pstate->norm_offset += BU_PTBL_LEN(&norms);
	bu_ptbl_free(&norms);
    }
    bu_free(region_name, "region name");
}


HIDDEN void
process_triangulation(struct conversion_state *pstate, struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the region to the TANKILL file */
	nmg_to_obj(pstate, r, pathp, tsp->ts_regionid, tsp->ts_aircode, tsp->ts_los, tsp->ts_gmater);

    } else {
	/* catch */

	char *sofar;

	sofar = db_path_to_string(pathp);
	bu_log("FAILED in triangulator: %s\n", sofar);
	bu_free((char *)sofar, "sofar");

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before bombing out.
	 */
	nmg_debug = pstate->nmg_debug;	/* restore mode */

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
obj_write_process_boolean(const struct conversion_state *pstate, union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp)
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

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before before bombing out.
	 */
	nmg_debug = pstate->nmg_debug;/* restore mode */

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
    union tree *ret_tree;
    struct bu_list vhead;
    struct nmgregion *r;
    struct conversion_state *pstate = (struct conversion_state *)client_data;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    BG_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK || pstate->gcv_options->verbosity_level) {
	char *sofar = db_path_to_string(pathp);
	bu_log("\ndo_region_end(%zu %zu%%) %s\n",
	       pstate->regions_tried,
	       (pstate->regions_tried>0) ? (pstate->regions_converted * 100) / pstate->regions_tried : 0,
	       sofar);
	bu_free(sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP)
	return curtree;

    pstate->regions_tried++;

    ret_tree = obj_write_process_boolean(pstate, curtree, tsp, pathp);

    if (ret_tree)
	r = ret_tree->tr_d.td_r;
    else
	r = (struct nmgregion *)NULL;

    pstate->regions_converted++;

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
	    process_triangulation(pstate, r, pathp, tsp);

	    pstate->regions_written++;

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


    db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

    if (pstate->regions_tried) {
	float npercent;
	float tpercent;

	npercent = (float)(pstate->regions_converted * 100) / pstate->regions_tried;
	tpercent = (float)(pstate->regions_written * 100) / pstate->regions_tried;
	bu_log("Tried %zu regions; %zu conv. to NMG's, %zu conv. to tri.; nmgper = %.2f%%, triper = %.2f%%\n",
	       pstate->regions_tried, pstate->regions_converted, pstate->regions_written, npercent, tpercent);
    }

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}


HIDDEN void
obj_write_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct obj_write_options *options_data;

    BU_ALLOC(options_data, struct obj_write_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(3 * sizeof(struct bu_opt_desc), "options_desc");

    BU_OPT((*options_desc)[0], NULL, "vertex-normals", NULL, NULL, &options_data->do_normals, "Output vertex normals.");
    BU_OPT((*options_desc)[1], NULL, "usemtl", NULL, NULL, &options_data->usemtl,
	    "Place usemtl statements in the output file. These statements are fictional "
	    "(they do not refer to any material database). The materials named provide "
	    "information about the material codes assigned to the objects in the BRLCAD database.");
    BU_OPT_NULL((*options_desc)[2]);
}


HIDDEN void
obj_write_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}


HIDDEN int
obj_write(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *dest_path)
{
    size_t i;
    struct model *the_model;
    struct db_tree_state tree_state;
    struct conversion_state state;

    memset(&state, 0, sizeof(state));
    state.gcv_options = gcv_options;
    state.obj_write_options = (struct obj_write_options *)options_data;
    state.nmg_debug = nmg_debug;

    if (!(state.fp = fopen(dest_path, "wb+"))) {
	perror("libgcv");
	bu_log("failed to open output file (%s)\n", dest_path);
	return 0;
    }

    tree_state = rt_initial_tree_state;
    tree_state.ts_tol = &state.gcv_options->calculational_tolerance;
    tree_state.ts_ttol = &state.gcv_options->tessellation_tolerance;
    tree_state.ts_m = &the_model;

    the_model = nmg_mm();
    BU_LIST_INIT(&RTG.rtg_vlfree);	/* for vlist macros */

    /* Write out header */
    if (NEAR_EQUAL(state.gcv_options->scale_factor, 1.0 / 25.4, RT_LEN_TOL))
	fprintf(state.fp, "# BRL-CAD generated Wavefront OBJ file (Units in)\n");
    else if (NEAR_EQUAL(state.gcv_options->scale_factor, 1.0, RT_LEN_TOL))
	fprintf(state.fp, "# BRL-CAD generated Wavefront OBJ file (Units mm)\n");
    else
	fprintf(state.fp, "# BRL-CAD generated Wavefront OBJ file (Units %lf/mm)\n", state.gcv_options->scale_factor);

    fprintf(state.fp, "# BRL-CAD model\n# BRL_CAD objects:");

    for (i = 0; i < state.gcv_options->num_objects; ++i)
	fprintf(state.fp, " %s", state.gcv_options->object_names[i]);

    fprintf(state.fp, "\n");

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(context->dbip, state.gcv_options->num_objects, (const char **)state.gcv_options->object_names,
	    1, &tree_state, NULL, do_region_end, nmg_booltree_leaf_tess, (void *)&state);

    if (state.regions_tried) {
	double percent = ((double)state.regions_converted * 100.0) / state.regions_tried;
	bu_log("Tried %zu regions, %zu converted to NMG's successfully.  %g%%\n",
	       state.regions_tried, state.regions_converted, percent);
	percent = ((double)state.regions_written * 100.0) / state.regions_tried;
	bu_log("                 %zu triangulated successfully. %g%%\n",
	       state.regions_written, percent);
    }

    fclose(state.fp);

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();

    return 1;
}

const struct gcv_filter gcv_conv_obj_write = {
    "OBJ Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_OBJ, NULL,
    obj_write_create_opts, obj_write_free_opts, obj_write
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
