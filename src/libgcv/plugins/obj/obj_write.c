/*                     O B J _ W R I T E . C
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

#include "bu/path.h"
#include "raytrace.h"
#include "rt/vlist.h"
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


struct obj_material_entry {
    struct bu_list l;
    char *name;
};


struct conversion_state
{
    const struct gcv_opts *gcv_options;
    const struct obj_write_options *obj_write_options;
    struct db_i *dbip;
    FILE *fp;
    FILE *mtl_fp;

    struct bu_list *vlfree;
    struct bu_list material_head;
    struct bu_vls material_lib_name;

    b_off_t vert_offset;
    b_off_t norm_offset;
    size_t regions_tried;
    size_t regions_converted;
    size_t regions_written;
    int nmg_debug;      /* saved for longjmp handling */
    int warned_missing_mtl;
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
obj_get_path_color(struct db_i *dbip, const struct db_full_path *pathp, unsigned char rgb[3])
{
    size_t i;
    int found = 0;

    if (!dbip || !pathp)
	return 0;

    for (i = 0; i < pathp->fp_len; ++i) {
	struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	const char *color_val;
	int inherit;

	if (db5_get_attributes(dbip, &avs, pathp->fp_names[i]) != 0)
	    continue;

	inherit = BU_STR_EQUAL(bu_avs_get(&avs, "inherit"), "1");

	if (db_mater_head(dbip)) {
	    int region_id = -1;
	    const char *region_id_val = bu_avs_get(&avs, "region_id");
	    const struct mater *mp;

	    if (region_id_val) {
		bu_opt_int(NULL, 1, &region_id_val, (void *)&region_id);
	    } else if (pathp->fp_names[i]->d_flags & RT_DIR_REGION) {
		region_id = 0;
	    }

	    if (region_id >= 0) {
		for (mp = db_mater_head(dbip); mp != MATER_NULL; mp = mp->mt_forw) {
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
obj_get_export_color(struct db_i *dbip, int region_id, const struct mater_info *mater, unsigned char rgb[3])
{
    struct region reg;

    if (mater->ma_color_valid) {
	obj_rgb_from_mater(rgb, mater);
	return 1;
    }

    if (!dbip)
	return 0;

    memset(&reg, 0, sizeof(reg));
    reg.reg_regionid = region_id;
    reg.reg_mater = *mater;
    db_mater_color_region(dbip, &reg);

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
	    bu_vls_sprintf(name, "%d_%d_%d__color_%u_%u_%u",
		    aircode, los, material_id, rgb[0], rgb[1], rgb[2]);
	} else {
	    bu_vls_sprintf(name, "color_%u_%u_%u", rgb[0], rgb[1], rgb[2]);
	}
	return 1;
    }

    if (preserve_gift) {
	bu_vls_sprintf(name, "%d_%d_%d", aircode, los, material_id);
	return 1;
    }

    return 0;
}


static int
obj_material_exists(const struct conversion_state *pstate, const char *name)
{
    struct obj_material_entry *entry;

    for (BU_LIST_FOR(entry, obj_material_entry, &pstate->material_head)) {
	if (BU_STR_EQUAL(entry->name, name))
	    return 1;
    }

    return 0;
}


static void
obj_write_material_definition(struct conversion_state *pstate, const char *name, int color_valid, const unsigned char rgb[3])
{
    struct obj_material_entry *entry;
    double kd[3] = {0.8, 0.8, 0.8};

    if (!pstate->mtl_fp || !name || obj_material_exists(pstate, name))
	return;

    if (color_valid) {
	kd[0] = ((double)rgb[0]) / 255.0;
	kd[1] = ((double)rgb[1]) / 255.0;
	kd[2] = ((double)rgb[2]) / 255.0;
    }

    fprintf(pstate->mtl_fp, "newmtl %s\n", name);
    fprintf(pstate->mtl_fp, "Ka %.6f %.6f %.6f\n", kd[0], kd[1], kd[2]);
    fprintf(pstate->mtl_fp, "Kd %.6f %.6f %.6f\n", kd[0], kd[1], kd[2]);
    fprintf(pstate->mtl_fp, "Ks 0.000000 0.000000 0.000000\n");
    fprintf(pstate->mtl_fp, "d 1.000000\n");
    fprintf(pstate->mtl_fp, "illum 1\n\n");

    BU_ALLOC(entry, struct obj_material_entry);
    BU_LIST_INIT(&entry->l);
    entry->name = bu_strdup(name);
    BU_LIST_APPEND(&pstate->material_head, &entry->l);
}


static void
obj_cleanup_materials(struct conversion_state *pstate)
{
    struct obj_material_entry *entry;

    if (pstate->mtl_fp) {
	fclose(pstate->mtl_fp);
	pstate->mtl_fp = NULL;
    }

    while (BU_LIST_WHILE(entry, obj_material_entry, &pstate->material_head)) {
	BU_LIST_DEQUEUE(&entry->l);
	bu_free(entry->name, "obj material name");
	bu_free(entry, "obj material entry");
    }

    bu_vls_free(&pstate->material_lib_name);
}


static void
obj_open_material_file(struct conversion_state *pstate, const char *obj_path)
{
    struct bu_vls dir = BU_VLS_INIT_ZERO;
    struct bu_vls base = BU_VLS_INIT_ZERO;
    struct bu_vls mtl_path = BU_VLS_INIT_ZERO;

    if (!obj_path || !strlen(obj_path))
	goto cleanup;

    if (!bu_path_component(&base, obj_path, BU_PATH_BASENAME_EXTLESS))
	goto cleanup;

    bu_vls_sprintf(&pstate->material_lib_name, "%s.mtl", bu_vls_addr(&base));

    if (bu_path_component(&dir, obj_path, BU_PATH_DIRNAME) &&
	    bu_vls_strlen(&dir) > 0 &&
	    !BU_STR_EQUAL(bu_vls_addr(&dir), ".")) {
	bu_vls_sprintf(&mtl_path, "%s/%s", bu_vls_addr(&dir), bu_vls_addr(&pstate->material_lib_name));
    } else {
	bu_vls_sprintf(&mtl_path, "%s", bu_vls_addr(&pstate->material_lib_name));
    }

    if ((pstate->mtl_fp = fopen(bu_vls_addr(&mtl_path), "wb")) == NULL) {
	perror("libgcv");
	bu_log("failed to open companion material file (%s)\n", bu_vls_addr(&mtl_path));
	bu_vls_trunc(&pstate->material_lib_name, 0);
	goto cleanup;
    }

    fprintf(pstate->mtl_fp, "# BRL-CAD generated Wavefront material file\n");

cleanup:
    bu_vls_free(&dir);
    bu_vls_free(&base);
    bu_vls_free(&mtl_path);
}


static void
obj_write_region_material(struct conversion_state *pstate, const struct db_full_path *pathp, const struct db_tree_state *tsp)
{
    struct bu_vls material_name = BU_VLS_INIT_ZERO;
    unsigned char rgb[3] = {0, 0, 0};
    struct db_i *idbip = tsp->ts_dbip ? tsp->ts_dbip : pstate->dbip;
    int color_valid = obj_get_path_color(idbip, pathp, rgb);

    if (!color_valid)
	color_valid = obj_get_export_color(idbip, tsp->ts_regionid, &tsp->ts_mater, rgb);

    if (pstate->mtl_fp) {
	if (obj_material_name(&material_name, color_valid, rgb,
		    pstate->obj_write_options->usemtl,
		    tsp->ts_aircode, tsp->ts_los, tsp->ts_gmater)) {
	    obj_write_material_definition(pstate, bu_vls_addr(&material_name), color_valid, rgb);
	    fprintf(pstate->fp, "usemtl %s\n", bu_vls_addr(&material_name));
	}
	bu_vls_free(&material_name);
	return;
    }

    if (color_valid && !pstate->warned_missing_mtl) {
	bu_log("gcv obj write: unable to export BRL-CAD colors without a companion .mtl file; continuing without color assignments\n");
	pstate->warned_missing_mtl = 1;
    }

    if (pstate->obj_write_options->usemtl)
	fprintf(pstate->fp, "usemtl %d_%d_%d\n", tsp->ts_aircode, tsp->ts_los, tsp->ts_gmater);
}


static void
nmg_to_obj(struct conversion_state *pstate, struct nmgregion *r, const struct db_full_path *pathp, const struct db_tree_state *tsp, struct bu_list *vlfree)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    struct bu_ptbl verts;
    struct bu_ptbl norms;
    char *region_name;
    size_t numverts = 0;		/* Number of vertices to output */
    size_t i;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, vlfree, &pstate->gcv_options->calculational_tolerance);

    /* list all vertices in result */
    nmg_vertex_tabulate(&verts, &r->l.magic, vlfree);

    /* Get number of vertices */
    numverts = BU_PTBL_LEN(&verts);

    /* get list of vertexuse normals */
    if (pstate->obj_write_options->do_normals)
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
	    }
	}
    }

    /* END CHECK SECTION */
    /* Write pertinent info for this region */

    obj_write_region_material(pstate, pathp, tsp);

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


static void
process_triangulation(struct conversion_state *pstate, struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the region to the TANKILL file */
	nmg_to_obj(pstate, r, pathp, tsp, pstate->vlfree);

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


static union tree *
obj_write_process_boolean(const struct conversion_state *pstate, union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp, struct bu_list *vlfree)
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
	nmg_debug = pstate->nmg_debug;/* restore mode */

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
static union tree *
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

    ret_tree = obj_write_process_boolean(pstate, curtree, tsp, pathp, pstate->vlfree);

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


    db_free_tree(curtree);		/* Does an nmg_kr() */

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


static void
obj_write_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct obj_write_options *options_data;

    BU_ALLOC(options_data, struct obj_write_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(3 * sizeof(struct bu_opt_desc), "options_desc");

    BU_OPT((*options_desc)[0], NULL, "vertex-normals", NULL, NULL, &options_data->do_normals, "Output vertex normals.");
    BU_OPT((*options_desc)[1], NULL, "usemtl", NULL, NULL, &options_data->usemtl,
	    "Place usemtl statements in the output file. When BRL-CAD colors are present, "
	    "a companion .mtl file is generated with matching RGB values. Material names "
	    "also preserve the BRL-CAD air/LOS/material codes.");
    BU_OPT_NULL((*options_desc)[2]);
}


static void
obj_write_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}


static int
obj_write(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *dest_path)
{
    size_t i;
    struct model *the_model;
    struct db_tree_state tree_state;
    struct conversion_state state;

    memset(&state, 0, sizeof(state));
    state.gcv_options = gcv_options;
    state.obj_write_options = (struct obj_write_options *)options_data;
    state.dbip = context->dbip;
    state.nmg_debug = nmg_debug;
    BU_LIST_INIT(&state.material_head);
    bu_vls_init(&state.material_lib_name);

    if (!(state.fp = fopen(dest_path, "wb+"))) {
	perror("libgcv");
	bu_log("failed to open output file (%s)\n", dest_path);
	bu_vls_free(&state.material_lib_name);
	return 0;
    }

    obj_open_material_file(&state, dest_path);

    RT_DBTS_INIT(&tree_state);
    tree_state.ts_tol = &state.gcv_options->calculational_tolerance;
    tree_state.ts_ttol = &state.gcv_options->tessellation_tolerance;
    tree_state.ts_m = &the_model;

    the_model = nmg_mm();
    state.vlfree = &rt_vlfree;

    /* Write out header */
    if (NEAR_EQUAL(state.gcv_options->scale_factor, 1.0 / 25.4, RT_LEN_TOL))
	fprintf(state.fp, "# BRL-CAD generated Wavefront OBJ file (Units in)\n");
    else if (NEAR_EQUAL(state.gcv_options->scale_factor, 1.0, RT_LEN_TOL))
	fprintf(state.fp, "# BRL-CAD generated Wavefront OBJ file (Units mm)\n");
    else
	fprintf(state.fp, "# BRL-CAD generated Wavefront OBJ file (Units %lf/mm)\n", state.gcv_options->scale_factor);
    if (state.mtl_fp)
	fprintf(state.fp, "mtllib %s\n", bu_vls_addr(&state.material_lib_name));

    fprintf(state.fp, "# BRL-CAD model\n# BRL_CAD objects:");

    for (i = 0; i < state.gcv_options->num_objects; ++i)
	fprintf(state.fp, " %s", state.gcv_options->object_names[i]);

    fprintf(state.fp, "\n");

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(context->dbip, state.gcv_options->num_objects, (const char **)state.gcv_options->object_names,
	    1, &tree_state, NULL, do_region_end, rt_booltree_leaf_tess, (void *)&state);

    if (state.regions_tried) {
	double percent = ((double)state.regions_converted * 100.0) / state.regions_tried;
	bu_log("Tried %zu regions, %zu converted to NMG's successfully.  %g%%\n",
	       state.regions_tried, state.regions_converted, percent);
	percent = ((double)state.regions_written * 100.0) / state.regions_tried;
	bu_log("                 %zu triangulated successfully. %g%%\n",
	       state.regions_written, percent);
    }

    fclose(state.fp);
    obj_cleanup_materials(&state);

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
