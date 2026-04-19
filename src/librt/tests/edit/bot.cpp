/*                          B O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file bot.cpp
 *
 * Test editing of BOT (bag of triangles) primitive parameters.
 *
 * Reference BOT: a simple tetrahedron with 4 vertices and 4 faces.
 * Vertices:
 *   v[0] = (0, 0, 0)
 *   v[1] = (1, 0, 0)
 *   v[2] = (0, 1, 0)
 *   v[3] = (0, 0, 1)
 * Faces: {0,1,2}, {0,1,3}, {0,2,3}, {1,2,3}
 * Keypoint = v[0] = (0,0,0).
 *
 * rt_bot_mat: applies MAT4X3PNT to each vertex.
 *
 * RT_PARAMS_EDIT_SCALE (scale=2, keypoint=v[0]=(0,0,0)):
 *   All vertices scale by 2: v[1]=(1,0,0)→(2,0,0), etc.
 *
 * RT_PARAMS_EDIT_TRANS (e_para=(5,5,5)):
 *   All vertices shift by (5,5,5): v[0]→(5,5,5), v[3]→(5,5,6).
 *
 * RT_PARAMS_EDIT_ROT (e_para=(5,5,5), keypoint=v[0]=(0,0,0)):
 *   v[0] stays at origin (it IS the keypoint). v[1] rotates.
 *
 * ECMD_BOT_MOVEV (move vertex 2 to (3,3,3)):
 *   b->bot_verts[0]=2, e_inpara=3, e_para=(3,3,3).
 *   v[2] → (3,3,3).
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"
#include "rt/primitives/bot.h"


/* ECMD constants from edbot.c */
#define ECMD_BOT_PICKV		30061
#define ECMD_BOT_PICKE		30062
#define ECMD_BOT_PICKT		30063
#define ECMD_BOT_MOVEV		30064
#define ECMD_BOT_MOVEE		30065
#define ECMD_BOT_MOVET		30066
#define ECMD_BOT_MODE		30067
#define ECMD_BOT_ORIENT		30068
#define ECMD_BOT_THICK		30069
#define ECMD_BOT_FMODE		30070
#define ECMD_BOT_FDEL		30071
#define ECMD_BOT_FLAGS		30072
#define ECMD_BOT_MOVEV_LIST	30073
#define ECMD_BOT_ESPLIT		30074
#define ECMD_BOT_FSPLIT		30075
#define ECMD_BOT_VERTEX_FUSE	30076
#define ECMD_BOT_FACE_FUSE	30077


struct directory *
make_bot(struct rt_wdb *wdbp)
{
    const char *objname = "bot";

    struct rt_bot_internal *bot;
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic       = RT_BOT_INTERNAL_MAGIC;
    bot->mode        = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags   = 0;

    /* Tetrahedron: 4 vertices, 4 faces */
    bot->num_vertices = 4;
    bot->vertices = (fastf_t *)bu_malloc(4 * 3 * sizeof(fastf_t), "bot vertices");
    VSET(&bot->vertices[0], 0, 0, 0);
    VSET(&bot->vertices[3], 1, 0, 0);
    VSET(&bot->vertices[6], 0, 1, 0);
    VSET(&bot->vertices[9], 0, 0, 1);

    bot->num_faces = 4;
    bot->faces = (int *)bu_malloc(4 * 3 * sizeof(int), "bot faces");
    bot->faces[0] = 0; bot->faces[1] = 1; bot->faces[2] = 2;
    bot->faces[3] = 0; bot->faces[4] = 1; bot->faces[5] = 3;
    bot->faces[6] = 0; bot->faces[7] = 2; bot->faces[8] = 3;
    bot->faces[9] = 1; bot->faces[10] = 2; bot->faces[11] = 3;

    bot->thickness   = NULL;
    bot->face_mode   = NULL;
    bot->num_normals = 0;
    bot->normals     = NULL;
    bot->num_face_normals = 0;
    bot->face_normals = NULL;
    bot->num_uvs     = 0;
    bot->uvs         = NULL;

    wdb_export(wdbp, objname, (void *)bot, ID_BOT, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create bot object\n");

    return dp;
}

static void
bot_reset(struct rt_edit *s, struct rt_bot_internal *bot, struct rt_bot_edit *b)
{
    /* Restore vertex positions (may have grown; reset count back to 4) */
    bot->num_vertices = 4;
    bot->vertices = (fastf_t *)bu_realloc(bot->vertices,
	    4 * 3 * sizeof(fastf_t), "bot vertices reset");
    VSET(&bot->vertices[0], 0, 0, 0);
    VSET(&bot->vertices[3], 1, 0, 0);
    VSET(&bot->vertices[6], 0, 1, 0);
    VSET(&bot->vertices[9], 0, 0, 1);

    /* Restore face topology (may have grown; reset count back to 4) */
    bot->num_faces = 4;
    bot->faces = (int *)bu_realloc(bot->faces,
	    4 * 3 * sizeof(int), "bot faces reset");
    bot->faces[0] = 0; bot->faces[1] = 1; bot->faces[2] = 2;
    bot->faces[3] = 0; bot->faces[4] = 1; bot->faces[5] = 3;
    bot->faces[6] = 0; bot->faces[7] = 2; bot->faces[8] = 3;
    bot->faces[9] = 1; bot->faces[10] = 2; bot->faces[11] = 3;

    b->bot_verts[0] = -1;
    b->bot_verts[1] = -1;
    b->bot_verts[2] = -1;

    VSETALL(s->e_keypoint, 0.0);
    MAT_IDN(s->acc_rot_sol);
    MAT_IDN(s->incr_change);
    s->acc_sc_sol = 1.0;
    s->e_inpara   = 0;
    s->es_scale   = 0.0;
    s->mv_context = 1;
    s->e_mvalid   = 0;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return BRLCAD_ERROR;

    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create database instance\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);

    struct directory *dp = make_bot(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size  = 73.3197;
    v->gv_isize = 1.0 / v->gv_size;
    v->gv_scale = 0.5 * v->gv_size;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width  = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;

    struct rt_bot_internal *bot =
	(struct rt_bot_internal *)s->es_int.idb_ptr;
    struct rt_bot_edit *b = (struct rt_bot_edit *)s->ipe_ptr;

    vect_t mousevec;

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE (scale=2 about keypoint v[0]=(0,0,0))
     *
     * rt_bot_mat with MAT4X3PNT: v[i] *= 2 for all vertices.
     *   v[0]=(0,0,0)→(0,0,0), v[3]=(0,0,1)→(0,0,2)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(s->e_keypoint, 0, 0, 0);
    b->bot_verts[0] = -1;
    b->bot_verts[1] = -1;
    b->bot_verts[2] = -1;

    rt_edit_process(s);
    {
	vect_t v0 = {0, 0, 0}, v3 = {0, 0, 2};
	if (!VNEAR_EQUAL(&bot->vertices[0], v0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(&bot->vertices[9], v3, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n"
		    "  v[0]=%g,%g,%g  v[3]=%g,%g,%g\n",
		    V3ARGS(&bot->vertices[0]), V3ARGS(&bot->vertices[9]));
	bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: v[0]=%g,%g,%g v[3]=%g,%g,%g\n",
	       V3ARGS(&bot->vertices[0]), V3ARGS(&bot->vertices[9]));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS (translate; keypoint (0,0,0) → e_para=(5,5,5))
     *
     * All vertices shift by (5,5,5).
     * v[0]=(0,0,0)→(5,5,5), v[3]=(0,0,1)→(5,5,6)
     * ================================================================*/
    bot_reset(s, bot, b);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    VSET(s->e_keypoint, 0, 0, 0);
    b->bot_verts[0] = -1;
    b->bot_verts[1] = -1;
    b->bot_verts[2] = -1;

    rt_edit_process(s);
    {
	vect_t v0 = {5, 5, 5}, v3 = {5, 5, 6};
	if (!VNEAR_EQUAL(&bot->vertices[0], v0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(&bot->vertices[9], v3, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n"
		    "  v[0]=%g,%g,%g  v[3]=%g,%g,%g\n",
		    V3ARGS(&bot->vertices[0]), V3ARGS(&bot->vertices[9]));
	bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: v[0]=%g,%g,%g v[3]=%g,%g,%g\n",
	       V3ARGS(&bot->vertices[0]), V3ARGS(&bot->vertices[9]));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_ROT (rotate about keypoint (0,0,0), angles=5,5,5)
     *
     * v[0] stays at origin. v[1]=(1,0,0) rotates.
     * ================================================================*/
    bot_reset(s, bot, b);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VSET(s->e_keypoint, 0, 0, 0);
    b->bot_verts[0] = -1;
    b->bot_verts[1] = -1;
    b->bot_verts[2] = -1;

    rt_edit_process(s);
    {
	vect_t exp_v0 = {0, 0, 0};
	if (!VNEAR_EQUAL(&bot->vertices[0], exp_v0, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k): v[0] moved: %g,%g,%g\n",
		    V3ARGS(&bot->vertices[0]));
	vect_t orig_v1 = {1, 0, 0};
	if (VNEAR_EQUAL(&bot->vertices[3], orig_v1, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k): v[1] not rotated (%g,%g,%g)\n",
		    V3ARGS(&bot->vertices[3]));
	bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: v[0]=%g,%g,%g v[1]=%g,%g,%g\n",
	       V3ARGS(&bot->vertices[0]), V3ARGS(&bot->vertices[3]));
    }

    /* ================================================================
     * ECMD_BOT_MOVEV (move vertex 2 to (3,3,3))
     *
     * b->bot_verts[0]=2 (vertex index), e_inpara=3, e_para=(3,3,3).
     * v[2] → (3,3,3).
     * ================================================================*/
    bot_reset(s, bot, b);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BOT_MOVEV);
    s->e_inpara = 3;
    VSET(s->e_para, 3, 3, 3);
    b->bot_verts[0] = 2;  /* vertex index to move */
    b->bot_verts[1] = -1;
    b->bot_verts[2] = -1;
    s->e_mvalid = 0;

    rt_edit_process(s);
    {
	vect_t exp_v2 = {3, 3, 3};
	if (!VNEAR_EQUAL(&bot->vertices[6], exp_v2, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_BOT_MOVEV failed: v[2]=%g,%g,%g\n",
		    V3ARGS(&bot->vertices[6]));
	bu_log("ECMD_BOT_MOVEV SUCCESS: v[2]=%g,%g,%g\n",
	       V3ARGS(&bot->vertices[6]));
    }

    /* ================================================================
     * ECMD_BOT_MOVEV_LIST: move two vertices by a common delta
     *
     * Delta = (1, 0, 0); vertices 0 and 1.
     * v[0] = (0,0,0) → (1,0,0)
     * v[1] = (1,0,0) → (2,0,0)
     * e_inpara=5: [0..2]=delta, [3..4]=vertex indices
     * ================================================================*/
    bot_reset(s, bot, b);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BOT_MOVEV_LIST);
    s->e_inpara = 5;
    s->e_para[0] = 1.0;  /* ΔX */
    s->e_para[1] = 0.0;  /* ΔY */
    s->e_para[2] = 0.0;  /* ΔZ */
    s->e_para[3] = 0.0;  /* vertex 0 */
    s->e_para[4] = 1.0;  /* vertex 1 */

    rt_edit_process(s);
    {
	vect_t exp_v0 = {1, 0, 0};
	vect_t exp_v1 = {2, 0, 0};
	if (!VNEAR_EQUAL(&bot->vertices[0], exp_v0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(&bot->vertices[3], exp_v1, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_BOT_MOVEV_LIST: v[0]=%g,%g,%g v[1]=%g,%g,%g\n",
		    V3ARGS(&bot->vertices[0]), V3ARGS(&bot->vertices[3]));
	bu_log("ECMD_BOT_MOVEV_LIST SUCCESS: v[0]=(%g,%g,%g) v[1]=(%g,%g,%g)\n",
	       V3ARGS(&bot->vertices[0]), V3ARGS(&bot->vertices[3]));
    }

    /* ================================================================
     * ECMD_BOT_ESPLIT: split edge v[0]–v[1] on fresh tetrahedron
     *
     * Tetrahedron has faces {0,1,2},{0,1,3},{0,2,3},{1,2,3}.
     * Faces containing edge 0–1: {0,1,2} and {0,1,3}.
     * After split:
     *   new vertex 4 = midpoint(v[0],v[1]) = (0.5,0,0)
     *   face {0,1,2} → {0,4,2} + {4,1,2}
     *   face {0,1,3} → {0,4,3} + {4,1,3}
     * num_vertices: 4→5   num_faces: 4→6
     * ================================================================*/
    bot_reset(s, bot, b);
    b->bot_verts[0] = 0;  /* edge start */
    b->bot_verts[1] = 1;  /* edge end */
    b->bot_verts[2] = -1;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BOT_ESPLIT);
    s->e_inpara = 0;

    rt_edit_process(s);
    if (bot->num_vertices != 5 || bot->num_faces != 6)
	bu_exit(1, "ERROR: ECMD_BOT_ESPLIT: expected 5 verts/6 faces, got %zu/%zu\n",
		bot->num_vertices, bot->num_faces);
    {
	vect_t exp_mid = {0.5, 0, 0};
	if (!VNEAR_EQUAL(&bot->vertices[4*3], exp_mid, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_BOT_ESPLIT: midpoint wrong: %g,%g,%g\n",
		    V3ARGS(&bot->vertices[4*3]));
	bu_log("ECMD_BOT_ESPLIT SUCCESS: %zu verts, %zu faces, midpoint=(%g,%g,%g)\n",
	       bot->num_vertices, bot->num_faces,
	       V3ARGS(&bot->vertices[4*3]));
    }

    /* ================================================================
     * ECMD_BOT_FSPLIT: split face {0,2,3} of a fresh tetrahedron
     *
     * Face {0,2,3}: v[0]=(0,0,0), v[2]=(0,1,0), v[3]=(0,0,1)
     * Centroid = (0, 1/3, 1/3)
     * After split: new vert 4 = centroid; 3 new faces replace face 2.
     * num_faces: 4→6   num_vertices: 4→5
     * ================================================================*/
    bot_reset(s, bot, b);
    /* Select face {0,2,3} */
    b->bot_verts[0] = 0;
    b->bot_verts[1] = 2;
    b->bot_verts[2] = 3;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BOT_FSPLIT);
    s->e_inpara = 0;

    rt_edit_process(s);
    if (bot->num_vertices != 5 || bot->num_faces != 6)
	bu_exit(1, "ERROR: ECMD_BOT_FSPLIT: expected 5 verts/6 faces, got %zu/%zu\n",
		bot->num_vertices, bot->num_faces);
    {
	vect_t exp_cen = {0.0, 1.0/3.0, 1.0/3.0};
	if (!VNEAR_EQUAL(&bot->vertices[4*3], exp_cen, 1e-9))
	    bu_exit(1, "ERROR: ECMD_BOT_FSPLIT: centroid wrong: %g,%g,%g\n",
		    V3ARGS(&bot->vertices[4*3]));
	bu_log("ECMD_BOT_FSPLIT SUCCESS: %zu verts, %zu faces, centroid=(%g,%g,%g)\n",
	       bot->num_vertices, bot->num_faces,
	       V3ARGS(&bot->vertices[4*3]));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    bot_reset(s, bot, b);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    mousevec[X] = 0.1; mousevec[Y] = -0.05; mousevec[Z] = 0;
    bu_vls_trunc(s->log_str, 0);
    int rot_xy_ret = (*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec);
    if (rot_xy_ret != BRLCAD_OK)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(xy) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(xy) SUCCESS: rotation applied via knob path\n");


    /* ================================================================
     * RT_MATRIX_EDIT_ROT: absolute matrix rotation from keyboard angles
     *
     * Set model_changes to a 30-deg rotation about X. Keypoint is at
     * the primitive's vertex (varies per primitive).
     * After the edit model_changes must differ from identity.
     * ================================================================*/
    bot_reset(s, bot, b);
    MAT_IDN(s->model_changes);
    MAT_IDN(s->acc_rot_sol);
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 30, 0, 0);   /* 30-deg rotation about X axis */
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
mat_t ident;
MAT_IDN(ident);
if (bn_mat_is_equal(s->model_changes, ident, &tol))
    bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT did not rotate model_changes\n");
mat_t expected_rot;
MAT_IDN(expected_rot);
bn_mat_angles(expected_rot, 30, 0, 0);
if (!bn_mat_is_equal(s->acc_rot_sol, expected_rot, &tol))
    bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT: acc_rot_sol not updated\n");
bu_log("RT_MATRIX_EDIT_ROT SUCCESS: model_changes rotated, acc_rot_sol updated\n");
    }

    /* ================================================================
     * RT_MATRIX_EDIT_TRANS_MODEL_XYZ: absolute model-space translation
     *
     * Start with identity model_changes, keypoint at origin.
     * After translating to (10,20,30): model_changes * (0,0,0) == (10,20,30).
     * ================================================================*/
    bot_reset(s, bot, b);
    MAT_IDN(s->model_changes);
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_TRANS_MODEL_XYZ);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VSET(s->e_keypoint, 0, 0, 0);
    s->local2base = 1.0;

    {
point_t kp_saved;
VMOVE(kp_saved, s->e_keypoint);

rt_edit_process(s);

point_t kp_world;
MAT4X3PNT(kp_world, s->model_changes, kp_saved);
vect_t expected = {10, 20, 30};
if (!VNEAR_EQUAL(kp_world, expected, VUNITIZE_TOL))
    bu_exit(1, "ERROR: RT_MATRIX_EDIT_TRANS_MODEL_XYZ failed: "
    "keypoint maps to (%g,%g,%g), expected (10,20,30)\n",
    V3ARGS(kp_world));
bu_log("RT_MATRIX_EDIT_TRANS_MODEL_XYZ SUCCESS: "
       "keypoint maps to (%g,%g,%g)\n", V3ARGS(kp_world));
    }

    /* ================================================================
     * ECMD_BOT_VERTEX_FUSE: inject a duplicate vertex then fuse
     * Add a duplicate of vertex 0 (0,0,0) as vertex 4, update one face
     * to use it.  After fuse, num_vertices should be 4 again.
     * ================================================================*/
    {
	struct rt_bot_internal *bot2 = (struct rt_bot_internal *)s->es_int.idb_ptr;
	/* Add duplicate vertex */
	bot2->num_vertices = 5;
	bot2->vertices = (fastf_t *)bu_realloc(bot2->vertices,
		5 * 3 * sizeof(fastf_t), "bot dup vert");
	VSET(&bot2->vertices[12], 0, 0, 0);  /* duplicate of vertex 0 */
	/* Point the last face at vertex 4 instead of 0 */
	bot2->faces[9] = 4;  /* was 1,2,3 → now 4,2,3 */

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BOT_VERTEX_FUSE);
	s->e_inpara = 0;
	bu_vls_trunc(s->log_str, 0);
	rt_edit_process(s);

	if (bot2->num_vertices != 4)
	    bu_exit(1, "ERROR: ECMD_BOT_VERTEX_FUSE: expected 4 vertices after fuse, got %zu\n",
		    bot2->num_vertices);
	bu_log("ECMD_BOT_VERTEX_FUSE SUCCESS: %zu vertices remain. %s",
	       bot2->num_vertices, bu_vls_cstr(s->log_str));
	bu_vls_trunc(s->log_str, 0);
    }

    /* ================================================================
     * ECMD_BOT_FACE_FUSE: inject a duplicate face then fuse
     * ================================================================*/
    {
	bot_reset(s, (struct rt_bot_internal *)s->es_int.idb_ptr,
		  (struct rt_bot_edit *)s->ipe_ptr);
	struct rt_bot_internal *bot3 = (struct rt_bot_internal *)s->es_int.idb_ptr;
	/* Add a duplicate of face 0 {0,1,2} as face 4 */
	bot3->num_faces = 5;
	bot3->faces = (int *)bu_realloc(bot3->faces,
		5 * 3 * sizeof(int), "bot dup face");
	bot3->faces[12] = 0; bot3->faces[13] = 1; bot3->faces[14] = 2;

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BOT_FACE_FUSE);
	s->e_inpara = 0;
	bu_vls_trunc(s->log_str, 0);
	rt_edit_process(s);

	if (bot3->num_faces != 4)
	    bu_exit(1, "ERROR: ECMD_BOT_FACE_FUSE: expected 4 faces after fuse, got %zu\n",
		    bot3->num_faces);
	bu_log("ECMD_BOT_FACE_FUSE SUCCESS: %zu faces remain. %s",
	       bot3->num_faces, bu_vls_cstr(s->log_str));
	bu_vls_trunc(s->log_str, 0);
    }

    rt_edit_destroy(s);
    db_close(dbip);
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
