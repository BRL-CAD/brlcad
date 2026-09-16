/*                          N M G . C P P
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
/** @file nmg.cpp
 *
 * Test editing of NMG primitives via the new ECMD_NMG_VPICK, ECMD_NMG_VMOVE,
 * ECMD_NMG_FPICK, and ECMD_NMG_FMOVE edit commands.
 *
 * Reference NMG: a tetrahedron with 4 vertices, 4 triangular faces.
 *   v[0] = (0,0,0)   v[1] = (1,0,0)   v[2] = (0,1,0)   v[3] = (0,0,1)
 * Faces (OT_SAME faceuses, 0-based index order):
 *   face 0: v0, v1, v2
 *   face 1: v0, v1, v3
 *   face 2: v0, v2, v3
 *   face 3: v1, v2, v3
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "nmg.h"
#include "rt/geom.h"
#include "rt/primitives/nmg.h"
#include "rt/rt_ecmds.h"
#include "wdb.h"

/* ECMD constants from ednmg.c */
#define ECMD_NMG_EPICK		11019
#define ECMD_NMG_EMOVE		11020
#define ECMD_NMG_EDEBUG		11021
#define ECMD_NMG_FORW		11022
#define ECMD_NMG_BACK		11023
#define ECMD_NMG_RADIAL		11024
#define ECMD_NMG_ESPLIT		11025
#define ECMD_NMG_EKILL		11026
#define ECMD_NMG_LEXTRU		11027
#define ECMD_NMG_VPICK		11028
#define ECMD_NMG_VMOVE		11029
#define ECMD_NMG_FPICK		11030
#define ECMD_NMG_FMOVE		11031
#define ECMD_NMG_LEXTRU_DIR	11032


/* ------------------------------------------------------------------ */
/* Build an NMG with a single triangular wire loop in the Z=0 plane.   */
/* The wire loop lives in the shell's lu_hd (not in any faceuse).       */
/* Vertices: v0=(0,0,0)  v1=(1,0,0)  v2=(0,1,0)                        */
/* ------------------------------------------------------------------ */

static struct directory *
make_nmg_wireloop(struct rt_wdb *wdbp)
{
    struct model *m = nmg_mm();
    struct nmgregion *r = nmg_mrsv(m);
    struct shell *sh = BU_LIST_FIRST(shell, &r->s_hd);

    /*
     * Build a wire loop (loopuse in sh->lu_hd, NOT in a faceuse).
     * Pattern: nmg_mlv → nmg_meonvu → nmg_eusplit × 2
     */
    struct loopuse *lu = nmg_mlv(&sh->l.magic,
				 (struct vertex *)NULL, OT_SAME);
    /* lu is now in sh->lu_hd with one vertex-use */

    struct vertexuse *vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
    /* Convert the single vertex-use to a self-edge-use loop */
    struct edgeuse *eu0 = nmg_meonvu(vu);
    /* eu0 → v0 (first vertex allocated) */
    struct vertex *v0 = eu0->vu_p->v_p;

    /* Insert v1 after eu0 */
    struct edgeuse *eu1 = nmg_eusplit((struct vertex *)NULL, eu0, 0);
    struct vertex *v1 = eu1->vu_p->v_p;

    /* Insert v2 after eu1 */
    struct edgeuse *eu2 = nmg_eusplit((struct vertex *)NULL, eu1, 0);
    struct vertex *v2 = eu2->vu_p->v_p;

    /* Assign coordinates: triangle in Z=0 plane */
    point_t pt0 = {0, 0, 0};
    point_t pt1 = {1, 0, 0};
    point_t pt2 = {0, 1, 0};
    nmg_vertex_gv(v0, pt0);
    nmg_vertex_gv(v1, pt1);
    nmg_vertex_gv(v2, pt2);

    const char *objname = "nmg_wire";
    mk_nmg(wdbp, objname, m);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: make_nmg_wireloop: db_lookup failed\n");
    return dp;
}


/* ------------------------------------------------------------------ */
/* Build a tetrahedron NMG using nmg_cmface / nmg_vertex_gv           */
/* ------------------------------------------------------------------ */

static struct directory *
make_nmg_tet(struct rt_wdb *wdbp)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;

    struct model *m = nmg_mm();
    struct nmgregion *r = nmg_mrsv(m);
    struct shell *s = BU_LIST_FIRST(shell, &r->s_hd);

    /* 4 vertices */
    struct vertex *verts[4] = {NULL, NULL, NULL, NULL};
    point_t pts[4] = {
	{0, 0, 0},
	{1, 0, 0},
	{0, 1, 0},
	{0, 0, 1},
    };

    /* nmg_cmface requires struct vertex **vt[] */
    /* Face 0: v0 v1 v2 */
    struct vertex **fv0[3] = {&verts[0], &verts[1], &verts[2]};
    struct faceuse *fu0 = nmg_cmface(s, fv0, 3);
    nmg_vertex_gv(verts[0], pts[0]);
    nmg_vertex_gv(verts[1], pts[1]);
    nmg_vertex_gv(verts[2], pts[2]);
    nmg_fu_planeeqn(fu0, &tol);

    /* Face 1: v0 v1 v3 */
    struct vertex **fv1[3] = {&verts[0], &verts[1], &verts[3]};
    struct faceuse *fu1 = nmg_cmface(s, fv1, 3);
    nmg_vertex_gv(verts[3], pts[3]);
    nmg_fu_planeeqn(fu1, &tol);

    /* Face 2: v0 v2 v3 */
    struct vertex **fv2[3] = {&verts[0], &verts[2], &verts[3]};
    struct faceuse *fu2 = nmg_cmface(s, fv2, 3);
    nmg_fu_planeeqn(fu2, &tol);

    /* Face 3: v1 v2 v3 */
    struct vertex **fv3[3] = {&verts[1], &verts[2], &verts[3]};
    struct faceuse *fu3 = nmg_cmface(s, fv3, 3);
    nmg_fu_planeeqn(fu3, &tol);

    (void)fu3;

    /* Fix face normals */
    struct bu_list vlfree;
    BU_LIST_INIT(&vlfree);
    nmg_fix_normals(s, &vlfree, &tol);
    /* free any vlists that nmg_fix_normals may have allocated */
    BV_FREE_VLIST(&vlfree, &vlfree);

    const char *objname = "nmg_tet";
    mk_nmg(wdbp, objname, m);  /* mk_nmg takes ownership of m */

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create nmg object\n");
    return dp;
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

    struct directory *dp = make_nmg_tet(wdbp);

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

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    if (!s)
	bu_exit(1, "ERROR: rt_edit_create failed\n");

    struct rt_nmg_edit *ne = (struct rt_nmg_edit *)s->ipe_ptr;
    struct model *m = (struct model *)s->es_int.idb_ptr;
    NMG_CK_MODEL(m);

    /* ================================================================
     * Initial state: es_v and es_fu should be NULL
     * ================================================================*/
    if (ne->es_v != NULL || ne->es_fu != NULL)
	bu_exit(1, "ERROR: initial state: es_v/es_fu not NULL\n");
    bu_log("NMG initial state SUCCESS: es_v=NULL es_fu=NULL\n");

    s->local2base = 1.0;
    s->base2local = 1.0;
    s->mv_context = 0;
    MAT_IDN(s->e_invmat);

    /* ================================================================
     * ECMD_NMG_VPICK: pick vertex-use 0 (first edgeuse vertex in face 0)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_NMG_VPICK);
    s->e_inpara = 1;
    s->e_para[0] = 0.0;  /* vertex-use index 0 */

    rt_edit_process(s);
    if (!ne->es_v)
	bu_exit(1, "ERROR: ECMD_NMG_VPICK: es_v not set\n");
    NMG_CK_VERTEX(ne->es_v);
    {
	point_t got;
	VMOVE(got, ne->es_v->vg_p->coord);
	bu_log("ECMD_NMG_VPICK SUCCESS: vertex at (%g,%g,%g)\n",
	       V3ARGS(got));
    }

    /* ================================================================
     * ECMD_NMG_VMOVE: move selected vertex to (2, 0, 0)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_NMG_VMOVE);
    s->e_inpara = 3;
    s->e_para[0] = 2.0;
    s->e_para[1] = 0.0;
    s->e_para[2] = 0.0;
    s->e_mvalid = 0;

    rt_edit_process(s);
    {
	vect_t exp = {2, 0, 0};
	if (!VNEAR_EQUAL(ne->es_v->vg_p->coord, exp, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_NMG_VMOVE: vertex at (%g,%g,%g), expected (2,0,0)\n",
		    V3ARGS(ne->es_v->vg_p->coord));
	bu_log("ECMD_NMG_VMOVE SUCCESS: vertex moved to (%g,%g,%g)\n",
	       V3ARGS(ne->es_v->vg_p->coord));
    }

    /* ================================================================
     * ECMD_NMG_FPICK: pick face 0 (first OT_SAME faceuse)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_NMG_FPICK);
    s->e_inpara = 1;
    s->e_para[0] = 0.0;  /* face index 0 */

    rt_edit_process(s);
    if (!ne->es_fu)
	bu_exit(1, "ERROR: ECMD_NMG_FPICK: es_fu not set\n");
    NMG_CK_FACEUSE(ne->es_fu);
    bu_log("ECMD_NMG_FPICK SUCCESS: faceuse selected\n");

    /* ================================================================
     * ECMD_NMG_FPICK out-of-range: es_fu should remain unchanged
     * ================================================================*/
    {
	struct faceuse *prev_fu = ne->es_fu;
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_NMG_FPICK);
	s->e_inpara = 1;
	s->e_para[0] = 9999.0;  /* out of range */
	bu_vls_trunc(s->log_str, 0);
	rt_edit_process(s);
	/* ecmd_nmg_fpick leaves es_fu unchanged on error */
	if (ne->es_fu != prev_fu)
	    bu_exit(1, "ERROR: ECMD_NMG_FPICK out-of-range changed es_fu\n");
	bu_log("ECMD_NMG_FPICK out-of-range correctly refused\n");
    }

    /* ================================================================
     * ECMD_NMG_FMOVE: translate all vertices of face 0 by (1, 0, 0)
     * After: all 3 vertices of face 0 move by (1, 0, 0).
     * ================================================================*/
    /* Re-pick face 0 to ensure es_fu is valid */
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_NMG_FPICK);
    s->e_inpara = 1;
    s->e_para[0] = 0.0;
    rt_edit_process(s);

    /* Record pre-move positions of the face vertices */
    point_t pre[3];
    {
	struct loopuse *lu = BU_LIST_FIRST(loopuse, &ne->es_fu->lu_hd);
	struct edgeuse *eu;
	int i = 0;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    VMOVE(pre[i], eu->vu_p->v_p->vg_p->coord);
	    i++;
	    if (i >= 3) break;
	}
    }

    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_NMG_FMOVE);
    s->e_inpara = 3;
    s->e_para[0] = 1.0;  /* ΔX */
    s->e_para[1] = 0.0;  /* ΔY */
    s->e_para[2] = 0.0;  /* ΔZ */

    rt_edit_process(s);
    {
	struct loopuse *lu = BU_LIST_FIRST(loopuse, &ne->es_fu->lu_hd);
	struct edgeuse *eu;
	int i = 0;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    vect_t delta;
	    VSUB2(delta, eu->vu_p->v_p->vg_p->coord, pre[i]);
	    vect_t exp_delta = {1, 0, 0};
	    if (!VNEAR_EQUAL(delta, exp_delta, VUNITIZE_TOL))
		bu_exit(1, "ERROR: ECMD_NMG_FMOVE: vertex %d delta (%g,%g,%g), expected (1,0,0)\n",
			i, V3ARGS(delta));
	    i++;
	    if (i >= 3) break;
	}
	bu_log("ECMD_NMG_FMOVE SUCCESS: all face vertices translated by (1,0,0)\n");
    }

    rt_edit_destroy(s);
    db_close(dbip);

    /* ================================================================
     * ECMD_NMG_LEXTRU_DIR test: extrude a wire loop along (0,0,1) by 2.
     *
     * We need a fresh rt_edit with a wire-loop NMG.  The wire loop is a
     * triangle in the Z=0 plane.  Extruding along +Z by 2 units produces
     * a prism; the result shell should have faceuses.
     * ================================================================*/
    {
	struct db_i *wdbip = db_open_inmem();
	if (wdbip == DBI_NULL)
	    bu_exit(1, "ERROR: LEXTRU_DIR: db_open_inmem failed\n");

	struct rt_wdb *wwdbp = wdb_dbopen(wdbip, RT_WDB_TYPE_DB_INMEM);

	struct directory *wdp = make_nmg_wireloop(wwdbp);

	struct db_full_path wfp;
	db_full_path_init(&wfp);
	db_add_node_to_full_path(&wfp, wdp);

	struct bview *wv;
	BU_GET(wv, struct bview);
	bv_init(wv, NULL);
	wv->gv_size  = 10.0;
	wv->gv_isize = 0.1;
	wv->gv_scale = 5.0;
	bv_update(wv);
	bu_vls_sprintf(&wv->gv_name, "default");
	wv->gv_width = wv->gv_height = 512;

	struct rt_edit *ws = rt_edit_create(&wfp, wdbip, &tol, wv);
	ws->mv_context = 0;
	ws->local2base = 1.0;
	ws->base2local = 1.0;
	MAT_IDN(ws->e_invmat);

	/* Set up LEXTRU_DIR: scans for the wire loop, copies it */
	bu_vls_trunc(ws->log_str, 0);
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_LEXTRU_DIR);

	if (bu_vls_strlen(ws->log_str) > 0) {
	    bu_exit(1, "ERROR: ECMD_NMG_LEXTRU_DIR setup failed: %s\n",
		    bu_vls_cstr(ws->log_str));
	}

	struct rt_nmg_edit *wne = (struct rt_nmg_edit *)ws->ipe_ptr;
	if (!wne->lu_copy)
	    bu_exit(1, "ERROR: ECMD_NMG_LEXTRU_DIR: lu_copy not set\n");
	if (!wne->es_s)
	    bu_exit(1, "ERROR: ECMD_NMG_LEXTRU_DIR: es_s not set\n");

	/* Extrude: dir=(0,0,1), dist=2 */
	ws->e_inpara = 4;
	ws->e_para[0] = 0.0;  /* dir X */
	ws->e_para[1] = 0.0;  /* dir Y */
	ws->e_para[2] = 1.0;  /* dir Z */
	ws->e_para[3] = 2.0;  /* distance */

	bu_vls_trunc(ws->log_str, 0);
	rt_edit_process(ws);

	/* After extrusion the shell should have at least one faceuse */
	if (BU_LIST_IS_EMPTY(&wne->es_s->fu_hd))
	    bu_exit(1, "ERROR: ECMD_NMG_LEXTRU_DIR: es_s has no faceuses after extrusion\n");

	bu_log("ECMD_NMG_LEXTRU_DIR SUCCESS: shell has faceuses after extrusion\n");

	rt_edit_destroy(ws);
	db_close(wdbip);
    }

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
