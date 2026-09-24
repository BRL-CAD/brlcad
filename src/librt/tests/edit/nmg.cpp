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
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "raytrace.h"
#include "nmg.h"
#include "rt/geom.h"
#include "rt/primitives/nmg.h"
#include "rt/rt_ecmds.h"
#include "wdb.h"
#include "test_utils.h"

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

enum {
    NMG_WIRE_VERTEX_COUNT = 3,
    NMG_TET_VERTEX_COUNT = 4,
    NMG_TET_FACE_COUNT = 4,
    NMG_TET_EDGEUSE_COUNT = 12,
    NMG_PRISM_VERTEX_COUNT = 2 * NMG_WIRE_VERTEX_COUNT,
    NMG_PRISM_FACE_COUNT = 5,
    NMG_PRISM_EDGEUSE_COUNT = 18,
    NMG_WIRE_AND_PRISM_VERTEX_COUNT = NMG_WIRE_VERTEX_COUNT + NMG_PRISM_VERTEX_COUNT,
    NMG_TWO_PRISM_VERTEX_COUNT = NMG_WIRE_VERTEX_COUNT + 2 * NMG_PRISM_VERTEX_COUNT
};


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

static bool
nmg_points_match(const struct rt_edit *edit, const point_t *expected,
		 size_t expected_count)
{
    const struct model *model = (const struct model *)edit->es_int.idb_ptr;
    struct bu_ptbl vertices = BU_PTBL_INIT_ZERO;
    nmg_vertex_tabulate(&vertices, &model->magic, edit->vlfree);
    bool same = BU_PTBL_LEN(&vertices) == expected_count;
    bool *matched = (bool *)bu_calloc(expected_count, sizeof(bool),
	"NMG expected vertices");
    for (size_t i = 0; same && i < BU_PTBL_LEN(&vertices); ++i) {
	const struct vertex *vertex =
	    (const struct vertex *)BU_PTBL_GET(&vertices, i);
	if (!vertex->vg_p) {
	    same = false;
	    break;
	}
	bool found = false;
	for (size_t j = 0; j < expected_count; ++j) {
	    if (!matched[j] &&
		VNEAR_EQUAL(vertex->vg_p->coord, expected[j], VUNITIZE_TOL)) {
		matched[j] = true;
		found = true;
		break;
	    }
	}
	if (!found)
	    same = false;
    }
    bu_free(matched, "NMG expected vertices");
    bu_ptbl_free(&vertices);
    return same;
}

static void
nmg_face_topology(const struct model *model, size_t *faces,
		  size_t *edgeuses)
{
    *faces = 0;
    *edgeuses = 0;
    struct nmgregion *region;
    for (BU_LIST_FOR(region, nmgregion, &model->r_hd)) {
	struct shell *shell;
	for (BU_LIST_FOR(shell, shell, &region->s_hd)) {
	    struct faceuse *face;
	    for (BU_LIST_FOR(face, faceuse, &shell->fu_hd)) {
		if (face->orientation != OT_SAME)
		    continue;
		++*faces;
		struct loopuse *loop;
		for (BU_LIST_FOR(loop, loopuse, &face->lu_hd)) {
		    if (BU_LIST_FIRST_MAGIC(&loop->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;
		    struct edgeuse *edge;
		    for (BU_LIST_FOR(edge, edgeuse, &loop->down_hd)) {
			++*edgeuses;
		    }
		}
	    }
	}
    }

}

static bool
nmg_tet_state(const struct rt_edit *edit,
	      const point_t expected[NMG_TET_VERTEX_COUNT],
	      int selected_vertex, bool face_selected)
{
    const struct model *model = (const struct model *)edit->es_int.idb_ptr;
    const struct rt_nmg_edit *selection =
	(const struct rt_nmg_edit *)edit->ipe_ptr;
    if (!model || !selection ||
	(selected_vertex < 0 ? selection->es_v != NULL :
	    !selection->es_v || !selection->es_v->vg_p ||
	    !VNEAR_EQUAL(selection->es_v->vg_p->coord,
		expected[selected_vertex], VUNITIZE_TOL)) ||
	(face_selected != (selection->es_fu != NULL)))
	return false;

    size_t faces, edgeuses;
    nmg_face_topology(model, &faces, &edgeuses);
    return faces == NMG_TET_FACE_COUNT &&
	edgeuses == NMG_TET_EDGEUSE_COUNT &&
	nmg_points_match(edit, expected, NMG_TET_VERTEX_COUNT);
}

static int
nmg_tet_step(struct rt_edit *edit, const char *unit, const char *name,
	     int command, const fastf_t *params, int count,
	     const point_t expected[NMG_TET_VERTEX_COUNT], int selected_vertex,
	     bool face_selected, bool reject = false)
{
    if (count < 0 || count > RT_EDIT_MAXPARA || (count && !params))
	return 1;
    rt_edit_set_edflag(edit, command);
    edit->e_inpara = count;
    for (int i = 0; i < count; ++i)
	edit->e_para[i] = params[i];
    int result = rt_edit_process(edit);
    bool same = nmg_tet_state(edit, expected, selected_vertex, face_selected);
    bool ok = (reject ? result != BRLCAD_OK : result == BRLCAD_OK) && same;
    for (int i = 0; ok && i < count; ++i)
	if (isnan(params[i]) ? !isnan(edit->e_para[i]) :
	    !EQUAL(edit->e_para[i], params[i]))
	    ok = false;
    bu_log("nmg\t%s\t%s\t%s\n", name, unit, ok ? "pass" : "fail");
    if (!ok)
	bu_log("NMG %s result=%d geometry=%d log=%s\n", name, result,
	    (int)same, bu_vls_cstr(edit->log_str));
    return ok ? 0 : 1;
}

static int
nmg_operation_matrix_unit(fastf_t local2base, const char *unit)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	return 1;
    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return 1;
    }
    struct directory *dp = make_nmg_tet(wdbp);
    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_edit *edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	db_free_full_path(&path);
	db_close(dbip);
	return 1;
    }
    edit->mv_context = 0;
    point_t expected[NMG_TET_VERTEX_COUNT] = {
	{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}
    };
    const fastf_t bad_pick[] = {0.5};
    const fastf_t bad_face[] = {NAN};
    const fastf_t pick[] = {0};
    const fastf_t vertex_target[] = {2.0 / local2base, 0, 0};
    const fastf_t bad_vertex_target[] = {NAN, 0, 0};
    const fastf_t face_delta[] = {25.4 / local2base, 0, 0};
    const fastf_t bad_face_delta[] = {NAN, 0, 0};
    const fastf_t extrude_direction[] = {0, 0, 1, 2.0 / local2base};
    int failures = 0;

    failures += nmg_tet_step(edit, unit, "reject edge move without selection",
	ECMD_NMG_EMOVE, vertex_target, 3, expected, -1, false, true);
    failures += nmg_tet_step(edit, unit, "reject edge split without selection",
	ECMD_NMG_ESPLIT, vertex_target, 3, expected, -1, false, true);
    failures += nmg_tet_step(edit, unit, "reject edge delete without selection",
	ECMD_NMG_EKILL, NULL, 0, expected, -1, false, true);
    failures += nmg_tet_step(edit, unit, "reject extrusion without loop",
	ECMD_NMG_LEXTRU_DIR, extrude_direction, 4, expected, -1, false, true);
    failures += nmg_tet_step(edit, unit, "reject fractional vertex index",
	ECMD_NMG_VPICK, bad_pick, 1, expected, -1, false, true);
    failures += nmg_tet_step(edit, unit, "pick vertex",
	ECMD_NMG_VPICK, pick, 1, expected, 1, false);
    failures += nmg_tet_step(edit, unit, "reject nonfinite vertex move",
	ECMD_NMG_VMOVE, bad_vertex_target, 3, expected, 1, false, true);
    expected[1][X] = 2;
    failures += nmg_tet_step(edit, unit, "move vertex",
	ECMD_NMG_VMOVE, vertex_target, 3, expected, 1, false);
    failures += nmg_tet_step(edit, unit, "reject nonfinite face index",
	ECMD_NMG_FPICK, bad_face, 1, expected, 1, false, true);
    failures += nmg_tet_step(edit, unit, "pick face",
	ECMD_NMG_FPICK, pick, 1, expected, 1, true);
    failures += nmg_tet_step(edit, unit, "reject nonfinite face delta",
	ECMD_NMG_FMOVE, bad_face_delta, 3, expected, 1, true, true);
    expected[1][X] += 25.4;
    expected[2][X] += 25.4;
    expected[3][X] += 25.4;
    failures += nmg_tet_step(edit, unit, "move face",
	ECMD_NMG_FMOVE, face_delta, 3, expected, 1, true);

    rt_edit_destroy(edit);
    db_free_full_path(&path);
    db_close(dbip);
    return failures;
}

static int
nmg_operation_matrix(void)
{
    return nmg_operation_matrix_unit(1.0, "mm") +
	nmg_operation_matrix_unit(25.4, "in");
}


int
rt_edit_test_nmg(void)
{
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
    s->local2base = 25.4;
    s->base2local = 1.0 / s->local2base;
    s->e_para[0] = 2.0 / s->local2base;
    s->e_para[1] = 0.0;
    s->e_para[2] = 0.0;
    s->e_mvalid = 0;

    rt_edit_process(s);
    if (!NEAR_EQUAL(s->e_para[0], 2.0 / s->local2base, VUNITIZE_TOL))
	bu_exit(1, "ERROR: NMG vertex move changed local-unit input\n");
    s->local2base = 1.0;
    s->base2local = 1.0;
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
    db_free_full_path(&fp);
    bv_free(v);
    db_close(dbip);

    /* ================================================================
     * ECMD_NMG_LEXTRU_DIR test: extrude a wire loop along (0,0,1) by 2.
     *
     * We need a fresh rt_edit with a wire-loop NMG.  The wire loop is a
     * triangle in the Z=0 plane.  Extruding along +Z by 2 units produces
     * a prism; the result shell should have faceuses.
     * ================================================================*/
    const fastf_t wire_units[] = {1.0, 25.4};
    for (size_t ui = 0; ui < sizeof(wire_units) / sizeof(wire_units[0]); ++ui) {
	const fastf_t local2base = wire_units[ui];
	struct db_i *wdbip = db_open_inmem();
	if (wdbip == DBI_NULL)
	    bu_exit(1, "ERROR: LEXTRU_DIR: db_open_inmem failed\n");
	wdbip->dbi_local2base = local2base;
	wdbip->dbi_base2local = 1.0 / local2base;

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
	if (!ws || !NEAR_EQUAL(ws->local2base, local2base, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: NMG wire edit did not inherit database units\n");
	ws->mv_context = 0;
	MAT_IDN(ws->e_invmat);
	struct rt_nmg_edit *wne = (struct rt_nmg_edit *)ws->ipe_ptr;
	struct model *wire_model = (struct model *)ws->es_int.idb_ptr;
	struct nmgregion *wire_region = BU_LIST_FIRST(nmgregion, &wire_model->r_hd);
	struct shell *wire_shell = BU_LIST_FIRST(shell, &wire_region->s_hd);
	struct loopuse *wire_loop = BU_LIST_FIRST(loopuse, &wire_shell->lu_hd);
	struct edgeuse *first_edge = BU_LIST_FIRST(edgeuse, &wire_loop->down_hd);
	point_t wire_expected[NMG_WIRE_VERTEX_COUNT] = {
	    {0, 0, 0}, {1, 0, 0}, {0, 1, 0}
	};
	if (!nmg_points_match(ws, wire_expected, NMG_WIRE_VERTEX_COUNT))
	    bu_exit(1, "ERROR: NMG wire fixture has unexpected vertices\n");
	point_t edge_midpoint, pick_view;
	VADD2SCALE(edge_midpoint, first_edge->vu_p->v_p->vg_p->coord,
		first_edge->eumate_p->vu_p->v_p->vg_p->coord, 0.5);
	MAT4X3PNT(pick_view, wv->gv_model2view, edge_midpoint);
	vect_t knob_state;
	VSET(knob_state, 7.0, 8.0, 9.0);
	VMOVE(ws->k.tra_m_abs, knob_state);
	VMOVE(ws->k.tra_v_abs, knob_state);
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_EPICK);
	if (EDOBJ[wdp->d_minor_type].ft_edit_xy(ws, pick_view) != BRLCAD_OK)
	    bu_exit(1, "ERROR: NMG mouse edge pick failed\n");
	if (!wne->es_eu)
	    bu_exit(1, "ERROR: NMG edge pick did not select an edge\n");
	point_t view_target;
	MAT4X3PNT(view_target, wv->gv_model2view, ws->curr_e_axes_pos);
	view_target[X] = pick_view[X];
	view_target[Y] = pick_view[Y];
	if (!edit_test_mouse_knobs_match(ws, view_target))
	    bu_exit(1, "ERROR: NMG mouse edge pick knobs missed cursor\n");

	struct edgeuse *picked_edge = wne->es_eu;
	struct edgeuse *next_edge = BU_LIST_PNEXT_CIRC(edgeuse, picked_edge);
	ws->e_inpara = 0;
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_FORW);
	if (wne->es_eu != next_edge)
	    bu_exit(1, "ERROR: NMG forward traversal selected the wrong edge\n");
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_BACK);
	if (wne->es_eu != picked_edge)
	    bu_exit(1, "ERROR: NMG backward traversal selected the wrong edge\n");
	struct edgeuse *radial_edge = picked_edge->eumate_p->radial_p;
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_RADIAL);
	if (wne->es_eu != radial_edge)
	    bu_exit(1, "ERROR: NMG radial traversal selected the wrong edge\n");
	point_t start_before, end_before, move_target, start_expected, end_expected;
	vect_t edge_dir, edge_shift;
	VMOVE(start_before, first_edge->vu_p->v_p->vg_p->coord);
	VMOVE(end_before, first_edge->eumate_p->vu_p->v_p->vg_p->coord);
	VSUB2(edge_dir, end_before, start_before);
	VSET(edge_shift, -edge_dir[Y], edge_dir[X], 0.0);
	VUNITIZE(edge_shift);
	VSCALE(edge_shift, edge_shift, 0.1);
	VADD2(move_target, edge_midpoint, edge_shift);
	VADD2(start_expected, start_before, edge_shift);
	VADD2(end_expected, end_before, edge_shift);
	bool found_start = false;
	bool found_end = false;
	for (point_t &point : wire_expected) {
	    if (VNEAR_EQUAL(point, start_before, VUNITIZE_TOL)) {
		VMOVE(point, start_expected);
		found_start = true;
	    } else if (VNEAR_EQUAL(point, end_before, VUNITIZE_TOL)) {
		VMOVE(point, end_expected);
		found_end = true;
	    }
	}
	if (!found_start || !found_end)
	    bu_exit(1, "ERROR: NMG wire edge endpoints not in fixture\n");
	wne->es_eu = first_edge;
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_EMOVE);
	ws->e_inpara = 3;
	VSCALE(ws->e_para, move_target, 1.0 / local2base);
	point_t move_input;
	VMOVE(move_input, ws->e_para);
	if (rt_edit_process(ws) != BRLCAD_OK)
	    bu_exit(1, "ERROR: NMG edge move failed\n");
	if (!VNEAR_EQUAL(ws->e_para, move_input, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: NMG edge move changed local-unit input\n");
	if (!VNEAR_EQUAL(first_edge->vu_p->v_p->vg_p->coord,
			 start_expected, VUNITIZE_TOL) ||
		!VNEAR_EQUAL(first_edge->eumate_p->vu_p->v_p->vg_p->coord,
			     end_expected, VUNITIZE_TOL) ||
		!nmg_points_match(ws, wire_expected, NMG_WIRE_VERTEX_COUNT))
	    bu_exit(1, "ERROR: NMG edge move: start=(%g,%g,%g), expected "
		    "(%g,%g,%g), end=(%g,%g,%g), expected (%g,%g,%g)\n",
		    V3ARGS(first_edge->vu_p->v_p->vg_p->coord),
		    V3ARGS(start_expected),
		    V3ARGS(first_edge->eumate_p->vu_p->v_p->vg_p->coord),
		    V3ARGS(end_expected));
	VMOVE(edge_midpoint, move_target);
	VSET(ws->e_para, NAN, 0, 0);
	ws->e_inpara = 3;
	if (rt_edit_process(ws) == BRLCAD_OK ||
	    !nmg_points_match(ws, wire_expected, NMG_WIRE_VERTEX_COUNT))
	    bu_exit(1, "ERROR: NMG accepted a nonfinite edge move\n");

	int edge_count = bu_list_len(&wire_loop->down_hd);
	wne->es_eu = first_edge;
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_ESPLIT);
	ws->e_inpara = 3;
	VSET(ws->e_para, NAN, 0, 0);
	if (rt_edit_process(ws) == BRLCAD_OK ||
	    bu_list_len(&wire_loop->down_hd) != edge_count ||
	    !nmg_points_match(ws, wire_expected, NMG_WIRE_VERTEX_COUNT))
	    bu_exit(1, "ERROR: NMG accepted a nonfinite edge split\n");
	VSCALE(ws->e_para, edge_midpoint, 1.0 / local2base);
	point_t split_input;
	VMOVE(split_input, ws->e_para);
	if (rt_edit_process(ws) != BRLCAD_OK)
	    bu_exit(1, "ERROR: NMG edge split failed\n");
	if (!VNEAR_EQUAL(ws->e_para, split_input, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: NMG edge split changed local-unit input\n");
	if (bu_list_len(&wire_loop->down_hd) != edge_count + 1 ||
		!wne->es_eu ||
		!VNEAR_EQUAL(wne->es_eu->vu_p->v_p->vg_p->coord,
			     edge_midpoint, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: NMG edge split did not insert the requested point\n");
	point_t split_expected[NMG_WIRE_VERTEX_COUNT + 1];
	for (size_t i = 0; i < NMG_WIRE_VERTEX_COUNT; ++i)
	    VMOVE(split_expected[i], wire_expected[i]);
	VMOVE(split_expected[NMG_WIRE_VERTEX_COUNT], edge_midpoint);
	if (!nmg_points_match(ws, split_expected,
		NMG_WIRE_VERTEX_COUNT + 1))
	    bu_exit(1, "ERROR: NMG edge split changed other vertices\n");
	bool replaced_end = false;
	for (point_t &point : wire_expected) {
	    if (VNEAR_EQUAL(point, end_expected, VUNITIZE_TOL)) {
		VMOVE(point, edge_midpoint);
		replaced_end = true;
		break;
	    }
	}
	if (!replaced_end)
	    bu_exit(1, "ERROR: NMG split edge endpoint not in fixture\n");

	/* Delete ignores parameter values left over from another edit. */
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_EKILL);
	ws->e_inpara = 3;
	VSCALE(ws->e_para, edge_midpoint, 1.0 / local2base);
	if (rt_edit_process(ws) != BRLCAD_OK)
	    bu_exit(1, "ERROR: NMG edge delete failed\n");
	if (bu_list_len(&wire_loop->down_hd) != edge_count ||
	    !nmg_points_match(ws, wire_expected, NMG_WIRE_VERTEX_COUNT))
	    bu_exit(1, "ERROR: NMG edge delete also changed another edge\n");

	wne->es_eu = NULL;

	/* Set up LEXTRU_DIR: scans for the wire loop, copies it */
	bu_vls_trunc(ws->log_str, 0);
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_LEXTRU_DIR);

	if (bu_vls_strlen(ws->log_str) > 0) {
	    bu_exit(1, "ERROR: ECMD_NMG_LEXTRU_DIR setup failed: %s\n",
		    bu_vls_cstr(ws->log_str));
	}

	if (!wne->lu_copy)
	    bu_exit(1, "ERROR: ECMD_NMG_LEXTRU_DIR: lu_copy not set\n");
	if (!wne->es_s)
	    bu_exit(1, "ERROR: ECMD_NMG_LEXTRU_DIR: es_s not set\n");

	/* Extrude two millimeters in either database unit system. */
	ws->e_inpara = 4;
	VSET(ws->e_para, 0, 0, 0);
	ws->e_para[3] = 2.0;
	if (rt_edit_process(ws) == BRLCAD_OK ||
	    !nmg_points_match(ws, wire_expected, NMG_WIRE_VERTEX_COUNT))
	    bu_exit(1, "ERROR: NMG accepted a zero extrusion direction\n");
	ws->e_para[0] = 0.0;  /* dir X */
	ws->e_para[1] = 0.0;  /* dir Y */
	ws->e_para[2] = 1.0;  /* dir Z */
	ws->e_para[3] = 2.0 / local2base;

	bu_vls_trunc(ws->log_str, 0);
	if (rt_edit_process(ws) != BRLCAD_OK)
	    bu_exit(1, "ERROR: NMG directed extrusion failed\n");

	if (!NEAR_EQUAL(ws->local2base, local2base, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(ws->e_para[Z], 1.0, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(ws->e_para[3], 2.0 / local2base, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_NMG_LEXTRU_DIR lost database units or extrusion distance\n");

	point_t prism_expected[NMG_PRISM_VERTEX_COUNT];
	for (size_t i = 0; i < NMG_WIRE_VERTEX_COUNT; ++i) {
	    VMOVE(prism_expected[i], wire_expected[i]);
	    VMOVE(prism_expected[i + NMG_WIRE_VERTEX_COUNT], wire_expected[i]);
	    prism_expected[i + NMG_WIRE_VERTEX_COUNT][Z] += 2.0;
	}
	point_t wire_and_prism_expected[NMG_WIRE_AND_PRISM_VERTEX_COUNT];
	for (size_t i = 0; i < NMG_WIRE_VERTEX_COUNT; ++i)
	    VMOVE(wire_and_prism_expected[i], wire_expected[i]);
	for (size_t i = 0; i < NMG_PRISM_VERTEX_COUNT; ++i)
	    VMOVE(wire_and_prism_expected[i + NMG_WIRE_VERTEX_COUNT],
		prism_expected[i]);
	if (!nmg_points_match(ws, wire_and_prism_expected,
		NMG_WIRE_AND_PRISM_VERTEX_COUNT))
	    bu_exit(1, "ERROR: NMG directed extrusion has unexpected vertices\n");
	size_t face_count, edgeuse_count;
	nmg_face_topology(wire_model, &face_count, &edgeuse_count);
	if (face_count != NMG_PRISM_FACE_COUNT ||
	    edgeuse_count != NMG_PRISM_EDGEUSE_COUNT)
	    bu_exit(1, "ERROR: NMG directed extrusion topology is %zu faces, %zu edges\n",
		face_count, edgeuse_count);
	ws->e_inpara = 4;
	VSET(ws->e_para, 0, 0, 0);
	ws->e_para[3] = 3.0 / local2base;
	if (rt_edit_process(ws) == BRLCAD_OK ||
	    !nmg_points_match(ws, wire_and_prism_expected,
		NMG_WIRE_AND_PRISM_VERTEX_COUNT))
	    bu_exit(1, "ERROR: NMG invalid preview target changed the solid\n");
	ws->e_para[Z] = 1.0;
	if (rt_edit_process(ws) != BRLCAD_OK)
	    bu_exit(1, "ERROR: NMG repeated extrusion failed\n");
	for (size_t i = NMG_WIRE_VERTEX_COUNT; i < NMG_PRISM_VERTEX_COUNT; ++i)
	    prism_expected[i][Z] = 3.0;
	for (size_t i = 0; i < NMG_PRISM_VERTEX_COUNT; ++i)
	    VMOVE(wire_and_prism_expected[i + NMG_WIRE_VERTEX_COUNT],
		prism_expected[i]);
	nmg_face_topology(wire_model, &face_count, &edgeuse_count);
	if (!nmg_points_match(ws, wire_and_prism_expected,
		NMG_WIRE_AND_PRISM_VERTEX_COUNT) ||
	    face_count != NMG_PRISM_FACE_COUNT ||
	    edgeuse_count != NMG_PRISM_EDGEUSE_COUNT)
	    bu_exit(1, "ERROR: NMG repeated extrusion did not replace its preview\n");

	bu_log("ECMD_NMG_LEXTRU_DIR SUCCESS: shell has faceuses after extrusion\n");

	/* The scalar-distance form must work after an earlier extrusion. */
	EDOBJ[wdp->d_minor_type].ft_set_edit_mode(ws, ECMD_NMG_LEXTRU);
	if (!wne->lu_copy || !wne->es_s)
	    bu_exit(1, "ERROR: NMG scalar extrusion setup failed\n");
	ws->e_inpara = 1;
	ws->e_para[0] = NAN;
	if (rt_edit_process(ws) == BRLCAD_OK ||
	    !nmg_points_match(ws, wire_and_prism_expected,
		NMG_WIRE_AND_PRISM_VERTEX_COUNT))
	    bu_exit(1, "ERROR: NMG accepted a nonfinite extrusion distance\n");
	ws->e_para[0] = 1.0 / local2base;
	if (rt_edit_process(ws) != BRLCAD_OK)
	    bu_exit(1, "ERROR: NMG scalar extrusion failed\n");
	point_t combined_expected[NMG_TWO_PRISM_VERTEX_COUNT];
	const size_t scalar_base = NMG_WIRE_AND_PRISM_VERTEX_COUNT;
	const size_t scalar_top = scalar_base + NMG_WIRE_VERTEX_COUNT;
	for (size_t i = 0; i < NMG_WIRE_VERTEX_COUNT; ++i) {
	    VMOVE(combined_expected[i], wire_expected[i]);
	    VMOVE(combined_expected[i + scalar_base], wire_expected[i]);
	    VMOVE(combined_expected[i + scalar_top], wire_expected[i]);
	    combined_expected[i + scalar_top][Z] -= 1.0;
	}
	for (size_t i = 0; i < NMG_PRISM_VERTEX_COUNT; ++i)
	    VMOVE(combined_expected[i + NMG_WIRE_VERTEX_COUNT],
		prism_expected[i]);
	nmg_face_topology(wire_model, &face_count, &edgeuse_count);
	if (!NEAR_EQUAL(ws->e_para[0], 1.0 / local2base, VUNITIZE_TOL) ||
	    !nmg_points_match(ws, combined_expected,
		NMG_TWO_PRISM_VERTEX_COUNT) ||
	    face_count != 2 * NMG_PRISM_FACE_COUNT ||
	    edgeuse_count != 2 * NMG_PRISM_EDGEUSE_COUNT)
	    bu_exit(1, "ERROR: NMG scalar extrusion lost units or geometry\n");

	EDOBJ[wdp->d_minor_type].ft_prim_edit_reset(ws);
	if (wne->lu_copy)
	    bu_exit(1, "ERROR: NMG edit reset retained the extrusion template\n");
	rt_edit_destroy(ws);
	db_free_full_path(&wfp);
	bv_free(wv);
	db_close(wdbip);
    }

    return nmg_operation_matrix() ? BRLCAD_ERROR : BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
