/*                         C O M B . C P P
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
/** @file comb.cpp
 *
 * Test combination (boolean tree) editing via the ECMD suite in edcomb.c.
 *
 * Test setup
 * ----------
 * We create three sphere solids and a combination "mybox" that unions them:
 *
 *   mybox = sphere0 u sphere1 u sphere2
 *
 * Then we exercise:
 *   ECMD_COMB_ADD_MEMBER   – append sphere3 with OP_SUBTRACT
 *   ECMD_COMB_DEL_MEMBER   – delete sphere1 (index 1)
 *   ECMD_COMB_SET_OP       – change sphere3 op to OP_INTERSECT
 *   ECMD_COMB_SET_MATRIX   – set an identity matrix on the first member
 *
 * The src_dbip / src_objname fields introduced in the upstream commit
 * d1dc6a4 are what allow edcomb.c to write changes back to the database.
 */

#include "common.h"

#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/nongeom.h"
#include "rt/tree.h"
#include "rt/op.h"
#include "wdb.h"
#include "rt/rt_ecmds.h"

/* ECMD constants (file-local in edcomb.c) */
#define ECMD_COMB_ADD_MEMBER   12001
#define ECMD_COMB_DEL_MEMBER   12002
#define ECMD_COMB_SET_OP       12003
#define ECMD_COMB_SET_MATRIX   12004
#define ECMD_COMB_SET_REGION   12005
#define ECMD_COMB_SET_COLOR    12006
#define ECMD_COMB_SET_SHADER   12007
#define ECMD_COMB_SET_MATERIAL 12008
#define ECMD_COMB_SET_REGION_ID 12009
#define ECMD_COMB_SET_AIRCODE  12010
#define ECMD_COMB_SET_GIFTMATER 12011
#define ECMD_COMB_SET_LOS      12012

/* rt_comb_edit struct (file-local in edcomb.c) */
struct rt_comb_edit {
    struct bu_vls es_name;
    int es_mat_valid;
    mat_t es_mat;
    struct bu_vls es_shader;
    struct bu_vls es_material;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Count how many OP_DB_LEAF nodes the tree has. */
static int
count_leaves(const union tree *tp)
{
    if (!tp) return 0;
    if (tp->tr_op == OP_DB_LEAF) return 1;
    return count_leaves(tp->tr_b.tb_left) + count_leaves(tp->tr_b.tb_right);
}


/* Reload the comb's rt_db_internal from the database so we see the
 * changes written by comb_write_back(). */
static void
reload_comb(struct rt_edit *s, const char *comb_name, struct db_i *dbip)
{
    struct directory *dp = db_lookup(dbip, comb_name, LOOKUP_QUIET);
    if (!dp) bu_exit(1, "ERROR: reload_comb: '%s' not found\n", comb_name);

    rt_db_free_internal(&s->es_int);
    RT_DB_INTERNAL_INIT(&s->es_int);
    if (rt_db_get_internal(&s->es_int, dp, dbip, NULL) < 0)
	bu_exit(1, "ERROR: reload_comb: rt_db_get_internal failed\n");
}


/* ------------------------------------------------------------------ */
/* Test setup                                                           */
/* ------------------------------------------------------------------ */

static struct db_i *g_dbip = NULL;

static void
make_sphere(struct rt_wdb *wdbp, const char *name, double x)
{
    point_t center;
    VSET(center, x, 0, 0);
    mk_sph(wdbp, name, center, 5.0);
}


static struct directory *
make_test_comb(struct rt_wdb *wdbp)
{
    /* Create member spheres */
    make_sphere(wdbp, "sphere0", 0);
    make_sphere(wdbp, "sphere1", 20);
    make_sphere(wdbp, "sphere2", 40);
    make_sphere(wdbp, "sphere3", 60);  /* used by ADD_MEMBER */

    /* Build comb: sphere0 u sphere1 u sphere2 */
    struct wmember wm;
    BU_LIST_INIT(&wm.l);
    mk_addmember("sphere0", &wm.l, NULL, WMOP_UNION);
    mk_addmember("sphere1", &wm.l, NULL, WMOP_UNION);
    mk_addmember("sphere2", &wm.l, NULL, WMOP_UNION);
    mk_lcomb(wdbp, "mybox", &wm, 0, NULL, NULL, NULL, 0);

    struct directory *dp = db_lookup(wdbp->dbip, "mybox", LOOKUP_QUIET);
    if (!dp) bu_exit(1, "ERROR: failed to look up 'mybox'\n");
    return dp;
}


/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return BRLCAD_ERROR;

    g_dbip = db_open_inmem();
    if (g_dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create in-memory database\n");

    struct rt_wdb *wdbp = wdb_dbopen(g_dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = make_test_comb(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size = 200.0;
    v->gv_isize = 1.0 / v->gv_size;
    v->gv_scale = 100.0;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width  = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, g_dbip, &tol, v);
    s->mv_context = 0;
    s->local2base = 1.0;

    struct rt_comb_edit *ce = (struct rt_comb_edit *)s->ipe_ptr;
    if (!ce)
	bu_exit(1, "ERROR: ipe_ptr not allocated\n");

    /* Confirm initial state: 3 leaves */
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	RT_CK_COMB(comb);
	int n = count_leaves(comb->tree);
	if (n != 3)
	    bu_exit(1, "ERROR: expected 3 initial leaves, got %d\n", n);
	bu_log("COMB initial state OK: %d members\n", n);
    }

    /* ================================================================
     * ECMD_COMB_ADD_MEMBER: add sphere3 with OP_SUBTRACT
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_ADD_MEMBER);
    bu_vls_sprintf(&ce->es_name, "sphere3");
    ce->es_mat_valid = 0;
    s->e_inpara = 1;
    s->e_para[0] = (fastf_t)OP_SUBTRACT;

    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    /* Reload and verify 4 leaves */
    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	int n = count_leaves(comb->tree);
	if (n != 4)
	    bu_exit(1, "ERROR: ADD_MEMBER: expected 4 leaves, got %d\n", n);
	bu_log("ECMD_COMB_ADD_MEMBER SUCCESS: %d members\n", n);
    }

    /* ================================================================
     * ECMD_COMB_DEL_MEMBER: delete index 1 (sphere1)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_DEL_MEMBER);
    s->e_inpara = 1;
    s->e_para[0] = 1.0;  /* delete member at index 1 */

    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	int n = count_leaves(comb->tree);
	if (n != 3)
	    bu_exit(1, "ERROR: DEL_MEMBER: expected 3 leaves, got %d\n", n);
	bu_log("ECMD_COMB_DEL_MEMBER SUCCESS: %d members remain\n", n);
    }

    /* ================================================================
     * ECMD_COMB_DEL_MEMBER out-of-range: index 99 should fail gracefully
     * ================================================================*/
    {
	int prev_leaves;
	{
	    struct rt_comb_internal *comb =
		(struct rt_comb_internal *)s->es_int.idb_ptr;
	    prev_leaves = count_leaves(comb->tree);
	}
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_DEL_MEMBER);
	s->e_inpara = 1;
	s->e_para[0] = 99.0;
	bu_vls_trunc(s->log_str, 0);
	rt_edit_process(s);
	struct rt_comb_internal *comb2 =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	int n = count_leaves(comb2->tree);
	if (n != prev_leaves)
	    bu_exit(1, "ERROR: DEL_MEMBER out-of-range changed leaf count\n");
	bu_log("ECMD_COMB_DEL_MEMBER out-of-range correctly refused (leaves=%d)\n", n);
	bu_vls_trunc(s->log_str, 0);
    }

    /* ================================================================
     * ECMD_COMB_SET_OP: change member at index 2 to OP_INTERSECT
     * After previous ops: sphere0(0), sphere2(1), sphere3(2)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_OP);
    s->e_inpara = 2;
    s->e_para[0] = 2.0;                    /* member index */
    s->e_para[1] = (fastf_t)OP_INTERSECT;  /* new op */

    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	int n = count_leaves(comb->tree);
	if (n != 3)
	    bu_exit(1, "ERROR: SET_OP: expected 3 leaves, got %d\n", n);
	bu_log("ECMD_COMB_SET_OP SUCCESS: tree has %d leaves\n", n);
    }

    /* ================================================================
     * ECMD_COMB_SET_MATRIX: set identity matrix on member index 0
     * e_para[0]=index, e_para[1..16]=16 matrix elements (needs MAXPARA>=20)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_MATRIX);
    s->e_inpara = 17;
    s->e_para[ 0] = 0.0;  /* member index 0 */
    /* Identity matrix row-major in e_para[1..16] */
    s->e_para[ 1] = 1; s->e_para[ 2] = 0; s->e_para[ 3] = 0; s->e_para[ 4] = 0;
    s->e_para[ 5] = 0; s->e_para[ 6] = 1; s->e_para[ 7] = 0; s->e_para[ 8] = 0;
    s->e_para[ 9] = 0; s->e_para[10] = 0; s->e_para[11] = 1; s->e_para[12] = 0;
    s->e_para[13] = 0; s->e_para[14] = 0; s->e_para[15] = 0; s->e_para[16] = 1;

    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	int n = count_leaves(comb->tree);
	if (n != 3)
	    bu_exit(1, "ERROR: SET_MATRIX: expected 3 leaves, got %d\n", n);
	bu_log("ECMD_COMB_SET_MATRIX SUCCESS: tree has %d leaves\n", n);
    }

    /* ================================================================
     * ECMD_COMB_ADD_MEMBER: invalid op should fail gracefully
     * ================================================================*/
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	int prev_leaves = count_leaves(comb->tree);

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_ADD_MEMBER);
	bu_vls_sprintf(&ce->es_name, "sphere0");
	s->e_inpara = 1;
	s->e_para[0] = 999.0;  /* invalid op */
	bu_vls_trunc(s->log_str, 0);
	rt_edit_process(s);
	bu_log("ECMD_COMB_ADD_MEMBER invalid op correctly refused\n");

	reload_comb(s, "mybox", g_dbip);
	comb = (struct rt_comb_internal *)s->es_int.idb_ptr;
	int n = count_leaves(comb->tree);
	if (n != prev_leaves)
	    bu_exit(1, "ERROR: ADD_MEMBER invalid op changed leaf count %d to %d\n",
		    prev_leaves, n);
	bu_vls_trunc(s->log_str, 0);
    }

    /* ================================================================
     * ECMD_COMB_SET_REGION: mark 'mybox' as a region
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_REGION);
    s->e_inpara = 1;
    s->e_para[0] = 1.0;   /* non-zero → region */
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (!comb->region_flag)
	    bu_exit(1, "ERROR: SET_REGION: region_flag not set\n");
	bu_log("ECMD_COMB_SET_REGION SUCCESS: region_flag=%d\n",
	       (int)comb->region_flag);
    }

    /* ================================================================
     * ECMD_COMB_SET_REGION_ID: set region_id = 42
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_REGION_ID);
    s->e_inpara = 1;
    s->e_para[0] = 42.0;
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (comb->region_id != 42)
	    bu_exit(1, "ERROR: SET_REGION_ID: got %ld, expected 42\n",
		    comb->region_id);
	bu_log("ECMD_COMB_SET_REGION_ID SUCCESS: region_id=%ld\n",
	       comb->region_id);
    }

    /* ================================================================
     * ECMD_COMB_SET_AIRCODE: set aircode = 7
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_AIRCODE);
    s->e_inpara = 1;
    s->e_para[0] = 7.0;
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (comb->aircode != 7)
	    bu_exit(1, "ERROR: SET_AIRCODE: got %ld, expected 7\n",
		    comb->aircode);
	bu_log("ECMD_COMB_SET_AIRCODE SUCCESS: aircode=%ld\n",
	       comb->aircode);
    }

    /* ================================================================
     * ECMD_COMB_SET_GIFTMATER: set GIFTmater = 5
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_GIFTMATER);
    s->e_inpara = 1;
    s->e_para[0] = 5.0;
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (comb->GIFTmater != 5)
	    bu_exit(1, "ERROR: SET_GIFTMATER: got %ld, expected 5\n",
		    comb->GIFTmater);
	bu_log("ECMD_COMB_SET_GIFTMATER SUCCESS: GIFTmater=%ld\n",
	       comb->GIFTmater);
    }

    /* ================================================================
     * ECMD_COMB_SET_LOS: set los = 80
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_LOS);
    s->e_inpara = 1;
    s->e_para[0] = 80.0;
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (comb->los != 80)
	    bu_exit(1, "ERROR: SET_LOS: got %ld, expected 80\n",
		    comb->los);
	bu_log("ECMD_COMB_SET_LOS SUCCESS: los=%ld\n", comb->los);
    }

    /* ================================================================
     * ECMD_COMB_SET_COLOR: set RGB = (255, 128, 0)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_COLOR);
    s->e_inpara = 3;
    s->e_para[0] = 255.0; s->e_para[1] = 128.0; s->e_para[2] = 0.0;
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (!comb->rgb_valid || comb->rgb[0] != 255 || comb->rgb[1] != 128 || comb->rgb[2] != 0)
	    bu_exit(1, "ERROR: SET_COLOR: rgb_valid=%d rgb=(%d,%d,%d)\n",
		    (int)comb->rgb_valid, (int)comb->rgb[0],
		    (int)comb->rgb[1], (int)comb->rgb[2]);
	bu_log("ECMD_COMB_SET_COLOR SUCCESS: rgb=(%d,%d,%d)\n",
	       (int)comb->rgb[0], (int)comb->rgb[1], (int)comb->rgb[2]);
    }

    /* ECMD_COMB_SET_COLOR clear: e_inpara=0 */
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_COLOR);
    s->e_inpara = 0;
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (comb->rgb_valid)
	    bu_exit(1, "ERROR: SET_COLOR clear: rgb_valid should be 0\n");
	bu_log("ECMD_COMB_SET_COLOR clear SUCCESS: rgb_valid=0\n");
    }

    /* ================================================================
     * ECMD_COMB_SET_SHADER: set shader string
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_SHADER);
    bu_vls_sprintf(&ce->es_shader, "plastic {sp 0.8 di 0.2}");
    s->e_inpara = 0;   /* shader is passed through es_shader, not e_para */
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (!BU_STR_EQUAL(bu_vls_cstr(&comb->shader), "plastic {sp 0.8 di 0.2}"))
	    bu_exit(1, "ERROR: SET_SHADER: got '%s'\n",
		    bu_vls_cstr(&comb->shader));
	bu_log("ECMD_COMB_SET_SHADER SUCCESS: shader='%s'\n",
	       bu_vls_cstr(&comb->shader));
    }

    /* ================================================================
     * ECMD_COMB_SET_MATERIAL: set material string
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_MATERIAL);
    bu_vls_sprintf(&ce->es_material, "air");
    s->e_inpara = 0;
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (!BU_STR_EQUAL(bu_vls_cstr(&comb->material), "air"))
	    bu_exit(1, "ERROR: SET_MATERIAL: got '%s'\n",
		    bu_vls_cstr(&comb->material));
	bu_log("ECMD_COMB_SET_MATERIAL SUCCESS: material='%s'\n",
	       bu_vls_cstr(&comb->material));
    }

    /* ================================================================
     * ECMD_COMB_SET_REGION clear: set region_flag back to 0
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_COMB_SET_REGION);
    s->e_inpara = 1;
    s->e_para[0] = 0.0;
    rt_edit_process(s);
    bu_vls_trunc(s->log_str, 0);

    reload_comb(s, "mybox", g_dbip);
    {
	struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)s->es_int.idb_ptr;
	if (comb->region_flag)
	    bu_exit(1, "ERROR: SET_REGION clear: region_flag should be 0\n");
	bu_log("ECMD_COMB_SET_REGION clear SUCCESS: region_flag=0\n");
    }

    rt_edit_destroy(s);
    db_close(g_dbip);
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
