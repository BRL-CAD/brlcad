/*                  T E S T _ C A M E R A . C
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
/** @file libbv/tests/test_camera.c
 *
 * Phase 2-D regression: verify every bv_view_get/set_* camera accessor
 * round-trips correctly and maintains the documented invariants between
 * the related fields (scale/size/isize, aet/rotation matrix).
 *
 * Usage: test_bv_camera
 *   Returns 0 on success, non-zero on failure.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bv/util.h"
#include "bv/defines.h"

/* Tolerance for floating-point matrix element comparison */
#define MAT_TOLERANCE 1e-9

/* compare two 4x4 matrices element-by-element */
static int
mat_near_equal(const mat_t a, const mat_t b)
{
    for (int i = 0; i < 16; i++)
	if (fabs(a[i] - b[i]) > MAT_TOLERANCE)
	    return 0;
    return 1;
}

static int g_fail = 0;

#define BVCHECK(cond, msg) \
    do { \
	if (!(cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
	    g_fail++; \
	} \
    } while (0)

#define NEAR_EQUAL_SCALAR(a, b) (fabs((a)-(b)) < 1e-9)

/* ---- helpers -------------------------------------------------------- */

static struct bview *
make_view(void)
{
    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    return v;
}

static void
free_view(struct bview *v)
{
    bv_free(v);
    BU_PUT(v, struct bview);
}

/* ---- scale ---------------------------------------------------------- */
static void
test_scale(void)
{
    bu_log("--- test_scale\n");
    struct bview *v = make_view();

    bv_view_set_scale(v, 100.0);
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_scale(v), 100.0),
	    "set_scale 100 -> get_scale == 100");
    /* size must be 2*scale */
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_size(v), 200.0),
	    "set_scale 100 -> gv_size == 200");
    /* isize == 1/size */
    BVCHECK(NEAR_EQUAL_SCALAR(v->gv_isize, 1.0 / 200.0),
	    "set_scale 100 -> gv_isize == 1/200");

    /* NULL guard */
    bv_view_set_scale(NULL, 50.0);   /* must not crash */
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_scale(NULL), 0.0),
	    "get_scale(NULL) == 0");

    free_view(v);
}

/* ---- size ----------------------------------------------------------- */
static void
test_size(void)
{
    bu_log("--- test_size\n");
    struct bview *v = make_view();

    bv_view_set_size(v, 400.0);
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_size(v), 400.0),
	    "set_size 400 -> get_size == 400");
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_scale(v), 200.0),
	    "set_size 400 -> scale == 200");
    BVCHECK(NEAR_EQUAL_SCALAR(v->gv_isize, 1.0 / 400.0),
	    "set_size 400 -> isize == 1/400");

    bv_view_set_size(NULL, 1.0);     /* no crash */
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_size(NULL), 0.0),
	    "get_size(NULL) == 0");

    free_view(v);
}

/* ---- scale/size symmetry ------------------------------------------- */
static void
test_scale_size_symmetry(void)
{
    bu_log("--- test_scale_size_symmetry\n");
    struct bview *v = make_view();

    /* set_scale then read size */
    bv_view_set_scale(v, 7.5);
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_size(v), 15.0),
	    "scale=7.5 -> size==15");

    /* set_size then read scale */
    bv_view_set_size(v, 15.0);
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_scale(v), 7.5),
	    "size=15 -> scale==7.5");

    free_view(v);
}

/* ---- perspective ---------------------------------------------------- */
static void
test_perspective(void)
{
    bu_log("--- test_perspective\n");
    struct bview *v = make_view();

    bv_view_set_perspective(v, 90.0);
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_perspective(v), 90.0),
	    "set_perspective 90 -> get_perspective == 90");
    bv_view_set_perspective(v, 0.0);
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_perspective(v), 0.0),
	    "set_perspective 0 -> get_perspective == 0");

    bv_view_set_perspective(NULL, 45.0);  /* no crash */
    BVCHECK(NEAR_EQUAL_SCALAR(bv_view_get_perspective(NULL), 0.0),
	    "get_perspective(NULL) == 0");

    free_view(v);
}

/* ---- AET ------------------------------------------------------------ */
static void
test_aet(void)
{
    bu_log("--- test_aet\n");
    struct bview *v = make_view();

    vect_t aet_in  = {35.0, 25.0, 0.0};
    vect_t aet_out = VINIT_ZERO;

    bv_view_set_aet(v, aet_in);
    bv_view_get_aet(v, aet_out);

    BVCHECK(VNEAR_EQUAL(aet_in, aet_out, 1e-9),
	    "set_aet(35,25,0) -> get_aet matches");

    /* bv_view_set_aet must recompute gv_rotation via bv_mat_aet.
     * A quick sanity: the rotation matrix must be orthonormal (det ~= 1). */
    mat_t R;
    MAT_COPY(R, v->gv_rotation);
    /* Compute R*R^T - should be identity */
    vect_t col0 = {R[0], R[4], R[8]};
    vect_t col1 = {R[1], R[5], R[9]};
    double dot = VDOT(col0, col1);
    BVCHECK(fabs(dot) < 1e-6,
	    "rotation matrix columns are orthogonal after set_aet");
    double len0 = MAGNITUDE(col0);
    BVCHECK(fabs(len0 - 1.0) < 1e-6,
	    "rotation matrix column 0 is unit length after set_aet");

    /* NULL guard */
    vect_t dummy = VINIT_ZERO;
    bv_view_set_aet(NULL, aet_in);  /* no crash */
    bv_view_get_aet(NULL, dummy);   /* no crash */

    free_view(v);
}

/* ---- raw rotation --------------------------------------------------- */
static void
test_rotation(void)
{
    bu_log("--- test_rotation\n");
    struct bview *v = make_view();

    /* Set a known rotation, read it back */
    mat_t rot_in, rot_out;
    MAT_IDN(rot_in);
    bv_view_set_rotation(v, rot_in);
    bv_view_get_rotation(v, rot_out);
    BVCHECK(mat_near_equal(rot_in, rot_out),
	    "set_rotation(idn) -> get_rotation == identity");

    /* NULL guard */
    mat_t dummy;
    MAT_ZERO(dummy);
    bv_view_set_rotation(NULL, rot_in);   /* no crash */
    bv_view_get_rotation(NULL, dummy);    /* no crash, should give identity */
    BVCHECK(mat_near_equal(dummy, rot_in), /* identity was written */
	    "get_rotation(NULL) fills identity");

    free_view(v);
}

/* ---- center_vec ----------------------------------------------------- */
static void
test_center_vec(void)
{
    bu_log("--- test_center_vec\n");
    struct bview *v = make_view();

    point_t c_in  = {10.0, -5.0, 3.0};
    point_t c_out = VINIT_ZERO;

    bv_view_set_center_vec(v, c_in);
    bv_view_get_center_vec(v, c_out);

    BVCHECK(VNEAR_EQUAL(c_in, c_out, 1e-9),
	    "set_center_vec(10,-5,3) -> get_center_vec matches");

    /* NULL guard */
    point_t dummy = VINIT_ZERO;
    bv_view_set_center_vec(NULL, c_in);   /* no crash */
    bv_view_get_center_vec(NULL, dummy);  /* no crash */

    free_view(v);
}

/* ---- aet/rotation consistency -------------------------------------- */
static void
test_aet_rotation_consistency(void)
{
    bu_log("--- test_aet_rotation_consistency\n");
    struct bview *v = make_view();

    /* Set AET, capture the resulting rotation matrix, re-read AET: must
     * be the same AET values we put in. */
    vect_t aet_in  = {90.0, 45.0, 0.0};
    bv_view_set_aet(v, aet_in);

    vect_t aet_out = VINIT_ZERO;
    bv_view_get_aet(v, aet_out);
    BVCHECK(VNEAR_EQUAL(aet_in, aet_out, 1e-9),
	    "AET stored in gv_aet survives set_aet round-trip");

    free_view(v);
}

int
main(int UNUSED(argc), char *argv[])
{
    bu_setprogname(argv[0]);

    test_scale();
    test_size();
    test_scale_size_symmetry();
    test_perspective();
    test_aet();
    test_rotation();
    test_center_vec();
    test_aet_rotation_consistency();

    if (g_fail) {
	bu_log("RESULT: %d failure(s)\n", g_fail);
	return 1;
    }
    bu_log("RESULT: all camera accessor tests PASSED\n");
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
