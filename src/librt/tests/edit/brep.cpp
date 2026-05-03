/*                       B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file brep.cpp
 *
 * Unit tests for BREP primitive editing via edbrep.cpp.
 *
 * Reference BREP: an ON_BrepSphere centred at (0,0,0) radius 10.
 * The sphere NURBS representation has 6 faces; each face's underlying
 * surface is an ON_NurbsSurface with at least 2×2 control vertices.
 *
 * Tests verify:
 *   - ECMD_BREP_SRF_SELECT stores face/i/j correctly
 *   - ECMD_BREP_SRF_CV_MOVE translates the CV by the given delta
 *   - ECMD_BREP_SRF_CV_SET places the CV at the given absolute position
 *   - rt_edit_brep_get_params returns sensible values
 *   - Invalid inputs are rejected gracefully (wrong e_inpara, bad face index)
 */

#include "common.h"

#include <cmath>
#include <cstring>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/rt_ecmds.h"
#include "brep/util.h"

/* Mirror the private struct from edbrep.cpp so the test can read state. */
struct rt_brep_edit_local {
    int face_index;
    int srf_cv_i;
    int srf_cv_j;
};


/* ------------------------------------------------------------------ *
 * Fixture helpers
 * ------------------------------------------------------------------ */

static struct directory *
make_brep_sphere(struct rt_wdb *wdbp)
{
    const char *objname = "brep_sph";

    ON_3dPoint centre(0, 0, 0);
    ON_Sphere sph(centre, 10.0);

    struct rt_brep_internal *bi;
    BU_ALLOC(bi, struct rt_brep_internal);
    bi->magic = RT_BREP_INTERNAL_MAGIC;
    bi->brep  = ON_BrepSphere(sph);

    /* ON_BrepSphere creates analytic (ON_RevSurface) surfaces.
     * Convert every non-NURBS surface in m_S[] to its NURBS form so
     * that CV editing commands (which require ON_NurbsSurface) work. */
    for (int i = 0; i < bi->brep->m_S.Count(); i++) {
	ON_Surface *old_srf = bi->brep->m_S[i];
	if (!dynamic_cast<ON_NurbsSurface *>(old_srf)) {
	    ON_NurbsSurface *ns = new ON_NurbsSurface;
	    if (old_srf->GetNurbForm(*ns, 0.0) > 0) {
		bi->brep->m_S[i] = ns;
		delete old_srf;
	    } else {
		delete ns;
	    }
	}
    }

    wdb_export(wdbp, objname, (void *)bi, ID_BREP, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: unable to create brep sphere object\n");

    return dp;
}

static struct rt_edit *
open_edit(struct directory *dp, struct db_i *dbip)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width  = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;
    return s;
}


/* ------------------------------------------------------------------ *
 * Helpers to read CV position from the edit state
 * ------------------------------------------------------------------ */

static void
get_cv_pos(struct rt_edit *s, int face_index, int cv_i, int cv_j,
	   double *x, double *y, double *z)
{
    struct rt_brep_internal *bip =
	(struct rt_brep_internal *)s->es_int.idb_ptr;
    RT_BREP_CK_MAGIC(bip);
    ON_Brep *brep = bip->brep;
    ON_BrepFace *face = brep->Face(face_index);
    const ON_Surface *surf = face->SurfaceOf();
    const ON_NurbsSurface *ns = dynamic_cast<const ON_NurbsSurface *>(surf);
    if (!ns)
	bu_exit(1, "ERROR: face %d has no NURBS surface\n", face_index);
    double *cv = ns->CV(cv_i, cv_j);
    *x = cv[0];
    *y = cv[1];
    *z = cv[2];
}


/* ------------------------------------------------------------------ *
 * Tests
 * ------------------------------------------------------------------ */

/* 1. SELECT stores indices correctly */
static void
test_brep_select(struct rt_edit *s)
{
    struct rt_brep_edit_local *b = (struct rt_brep_edit_local *)s->ipe_ptr;

    /* Select face 0, CV (0, 0) */
    EDOBJ[ID_BREP].ft_set_edit_mode(s, ECMD_BREP_SRF_SELECT);
    s->e_inpara = 3;
    s->e_para[0] = 0.0; /* face_index */
    s->e_para[1] = 0.0; /* cv_i */
    s->e_para[2] = 0.0; /* cv_j */

    rt_edit_process(s);

    if (b->face_index != 0 || b->srf_cv_i != 0 || b->srf_cv_j != 0)
	bu_exit(1,
		"ERROR: ECMD_BREP_SRF_SELECT: expected (0,0,0) got (%d,%d,%d)\n",
		b->face_index, b->srf_cv_i, b->srf_cv_j);

    bu_log("ECMD_BREP_SRF_SELECT (0,0,0) PASS\n");
}

/* 2. SELECT rejects invalid face index */
static void
test_brep_select_invalid_face(struct rt_edit *s)
{
    struct rt_brep_edit_local *b = (struct rt_brep_edit_local *)s->ipe_ptr;
    /* Reset to known state */
    b->face_index = -1; b->srf_cv_i = -1; b->srf_cv_j = -1;

    EDOBJ[ID_BREP].ft_set_edit_mode(s, ECMD_BREP_SRF_SELECT);
    s->e_inpara = 3;
    s->e_para[0] = 9999.0; /* out-of-range face */
    s->e_para[1] = 0.0;
    s->e_para[2] = 0.0;

    bu_vls_trunc(s->log_str, 0);
    rt_edit_process(s);

    if (b->face_index != -1)
	bu_exit(1,
		"ERROR: ECMD_BREP_SRF_SELECT invalid face did not reject: "
		"face_index=%d\n", b->face_index);

    bu_log("ECMD_BREP_SRF_SELECT invalid face PASS (rejected as expected)\n");
}

/* 3. SELECT rejects when e_inpara != 3 */
static void
test_brep_select_wrong_inpara(struct rt_edit *s)
{
    struct rt_brep_edit_local *b = (struct rt_brep_edit_local *)s->ipe_ptr;
    b->face_index = -1; b->srf_cv_i = -1; b->srf_cv_j = -1;

    EDOBJ[ID_BREP].ft_set_edit_mode(s, ECMD_BREP_SRF_SELECT);
    s->e_inpara = 1; /* wrong */
    s->e_para[0] = 0.0;

    rt_edit_process(s);

    if (b->face_index != -1)
	bu_exit(1,
		"ERROR: ECMD_BREP_SRF_SELECT wrong e_inpara did not reject\n");

    bu_log("ECMD_BREP_SRF_SELECT wrong e_inpara PASS\n");
}

/* 4. MOVE translates the selected CV */
static void
test_brep_cv_move(struct rt_edit *s)
{
    struct rt_brep_edit_local *b = (struct rt_brep_edit_local *)s->ipe_ptr;

    /* First select face 0 CV (1, 1) */
    EDOBJ[ID_BREP].ft_set_edit_mode(s, ECMD_BREP_SRF_SELECT);
    s->e_inpara = 3;
    s->e_para[0] = 0.0; /* face 0 */
    s->e_para[1] = 1.0; /* cv_i = 1 */
    s->e_para[2] = 1.0; /* cv_j = 1 */
    rt_edit_process(s);

    if (b->face_index != 0 || b->srf_cv_i != 1 || b->srf_cv_j != 1)
	bu_exit(1, "ERROR: ECMD_BREP_SRF_SELECT (0,1,1) failed\n");

    double x0, y0, z0;
    get_cv_pos(s, 0, 1, 1, &x0, &y0, &z0);

    /* Translate by (1, 2, 3) */
    EDOBJ[ID_BREP].ft_set_edit_mode(s, ECMD_BREP_SRF_CV_MOVE);
    s->e_inpara = 3;
    s->e_para[0] = 1.0;
    s->e_para[1] = 2.0;
    s->e_para[2] = 3.0;
    rt_edit_process(s);

    double x1, y1, z1;
    get_cv_pos(s, 0, 1, 1, &x1, &y1, &z1);

    /* local2base == 1.0 so delta in model units */
    double ex = x0 + 1.0;
    double ey = y0 + 2.0;
    double ez = z0 + 3.0;

    if (fabs(x1 - ex) > 1e-6 || fabs(y1 - ey) > 1e-6 || fabs(z1 - ez) > 1e-6)
	bu_exit(1,
		"ERROR: ECMD_BREP_SRF_CV_MOVE: expected (%.6f,%.6f,%.6f) "
		"got (%.6f,%.6f,%.6f)\n",
		ex, ey, ez, x1, y1, z1);

    bu_log("ECMD_BREP_SRF_CV_MOVE PASS: delta (1,2,3) applied correctly\n");
}

/* 5. SET places the CV at an absolute position */
static void
test_brep_cv_set(struct rt_edit *s)
{
    struct rt_brep_edit_local *b = (struct rt_brep_edit_local *)s->ipe_ptr;

    /* Select face 0 CV (0, 1) */
    EDOBJ[ID_BREP].ft_set_edit_mode(s, ECMD_BREP_SRF_SELECT);
    s->e_inpara = 3;
    s->e_para[0] = 0.0;
    s->e_para[1] = 0.0;
    s->e_para[2] = 1.0; /* cv_j = 1 */
    rt_edit_process(s);

    if (b->face_index != 0 || b->srf_cv_j != 1)
	bu_exit(1, "ERROR: ECMD_BREP_SRF_SELECT (0,0,1) failed\n");

    /* Place at (5, 5, 5) */
    EDOBJ[ID_BREP].ft_set_edit_mode(s, ECMD_BREP_SRF_CV_SET);
    s->e_inpara = 3;
    s->e_para[0] = 5.0;
    s->e_para[1] = 5.0;
    s->e_para[2] = 5.0;
    rt_edit_process(s);

    double xc, yc, zc;
    get_cv_pos(s, 0, 0, 1, &xc, &yc, &zc);

    if (fabs(xc - 5.0) > 1e-6 || fabs(yc - 5.0) > 1e-6 || fabs(zc - 5.0) > 1e-6)
	bu_exit(1,
		"ERROR: ECMD_BREP_SRF_CV_SET: expected (5,5,5) "
		"got (%.6f,%.6f,%.6f)\n", xc, yc, zc);

    bu_log("ECMD_BREP_SRF_CV_SET PASS: CV placed at (5,5,5)\n");
}

/* 6. MOVE rejected when no CV is selected */
static void
test_brep_cv_move_no_selection(struct rt_edit *s)
{
    struct rt_brep_edit_local *b = (struct rt_brep_edit_local *)s->ipe_ptr;
    /* Force clear selection */
    b->face_index = -1; b->srf_cv_i = -1; b->srf_cv_j = -1;

    EDOBJ[ID_BREP].ft_set_edit_mode(s, ECMD_BREP_SRF_CV_MOVE);
    s->e_inpara = 3;
    s->e_para[0] = s->e_para[1] = s->e_para[2] = 1.0;

    bu_vls_trunc(s->log_str, 0);
    rt_edit_process(s);

    /* Verify the operation was a no-op (no crash, CV state unchanged).
     * log_str is cleared by rt_edit_process so we cannot inspect it here. */
    if (b->face_index != -1 || b->srf_cv_i != -1 || b->srf_cv_j != -1)
	bu_exit(1,
		"ERROR: ECMD_BREP_SRF_CV_MOVE no-selection: state changed "
		"unexpectedly: face=%d i=%d j=%d\n",
		b->face_index, b->srf_cv_i, b->srf_cv_j);

    bu_log("ECMD_BREP_SRF_CV_MOVE no-selection PASS\n");
}

/* 7. get_params returns the current selection */
static void
test_brep_get_params_select(struct rt_edit *s)
{
    /* Select face 0 CV (1, 2) */
    EDOBJ[ID_BREP].ft_set_edit_mode(s, ECMD_BREP_SRF_SELECT);
    s->e_inpara = 3;
    s->e_para[0] = 0.0;
    s->e_para[1] = 1.0;
    s->e_para[2] = 2.0;
    rt_edit_process(s);

    fastf_t vals[3] = {0, 0, 0};
    int n = EDOBJ[ID_BREP].ft_edit_get_params(s, ECMD_BREP_SRF_SELECT, vals);

    if (n != 3 || (int)vals[0] != 0 || (int)vals[1] != 1 || (int)vals[2] != 2)
	bu_exit(1,
		"ERROR: get_params(SELECT) returned n=%d vals=(%.0f,%.0f,%.0f)\n",
		n, vals[0], vals[1], vals[2]);

    bu_log("get_params(ECMD_BREP_SRF_SELECT) PASS: (0,1,2)\n");
}

/* 8. Descriptor is well-formed */
static void
test_brep_edit_desc(struct directory *dp)
{
    const struct rt_edit_prim_desc *desc =
	EDOBJ[dp->d_minor_type].ft_edit_desc();

    if (!desc)
	bu_exit(1, "ERROR: rt_edit_brep_edit_desc() returned NULL\n");

    if (!desc->prim_type || !BU_STR_EQUAL(desc->prim_type, "brep"))
	bu_exit(1, "ERROR: prim_type is '%s', expected 'brep'\n",
		desc->prim_type ? desc->prim_type : "(null)");

    if (desc->ncmd != 3)
	bu_exit(1, "ERROR: ncmd=%d, expected 3\n", desc->ncmd);

    bu_log("rt_edit_brep_edit_desc PASS: prim_type='%s' ncmd=%d\n",
	   desc->prim_type, desc->ncmd);
}


/* ------------------------------------------------------------------ *
 * main
 * ------------------------------------------------------------------ */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return BRLCAD_ERROR;

    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: db_open_inmem failed\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = make_brep_sphere(wdbp);

    struct rt_edit *s = open_edit(dp, dbip);

    test_brep_edit_desc(dp);
    test_brep_select(s);
    test_brep_select_invalid_face(s);
    test_brep_select_wrong_inpara(s);
    test_brep_cv_move(s);
    test_brep_cv_set(s);
    test_brep_cv_move_no_selection(s);
    test_brep_get_params_select(s);

    rt_edit_destroy(s);
    db_close(dbip);

    bu_log("ALL brep edit tests PASSED\n");
    return 0;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
